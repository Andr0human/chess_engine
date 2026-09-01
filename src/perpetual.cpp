#include <algorithm>
#include <climits>
#include <unordered_map>

#include "perpetual.h"
#include "movegen.h"
#include "lookup_table.h"
#include "search_utils.h"


namespace {

// Dependency sentinel: this result leans on no path repetition at all.
constexpr int NO_PATH_DEP = INT_MAX;


/**
 * One resolved position.
 *
 * TRUE entries are unconditional. A forced repetition is proven or it is not,
 * and how much room was left when it was found does not enter into it.
 *
 * FALSE entries carry the room they failed with: `remaining` is plyCap - ply at
 * the node that established the failure. A query with no more room than that
 * cannot do better, so the entry is reusable only when the caller's remaining
 * is <= this. Same idea as a transposition table's depth field, counted from
 * the other end -- and it is what lets one iterative-deepening pass reuse the
 * shallower passes instead of only itself.
 */
struct CacheEntry
{
  bool    result;
  int16_t remaining;
};


struct ProveContext
{
  Color    attacker;
  uint64_t nodeBudget;
  int      plyCap;
  bool     useCache;
  uint64_t cacheCap;
  CheckOrder   order;
  EvasionOrder evasion;

  std::unordered_map<Key, CacheEntry> resolved;

  // hashValue at each search ply, used to find WHICH ancestor a repetition
  // closed onto. ChessBoard's undoInfo stack already knows this but does not
  // expose the index, and this array holds the search plies only -- exactly the
  // set the graph-history rule below cares about.
  std::array<Key, PERPETUAL_PLY_LIMIT> pathHash;
};


/**
 * The cache key.
 *
 * hashValue covers placement, side to move, castling and en passant -- but not
 * the halfmove clock, and here the clock is load-bearing, because
 * fiftyMoveDraw() is one of the prover's terminals. Two routes reaching the
 * same placement with different clocks have genuinely different sub-proofs and
 * must not share an entry.
 */
inline Key
cacheKey(const ChessBoard& pos)
{
  return pos.hashValue ^ (Key(pos.halfMoveClock()) * 0x9E3779B97F4A7C15ull);
}


/**
 * Ply of the deepest search ancestor holding this same position, or
 * NO_PATH_DEP if the repetition closed onto pre-root game history instead.
 *
 * Deepest rather than shallowest: the shorter cycle carries the weaker
 * dependency, and a weaker dependency is discharged lower in the tree, which is
 * where entries start becoming storable.
 *
 * Pre-root matches report NO_PATH_DEP deliberately. That history is on the path
 * at every node of the run and never leaves it, so a result resting on it is
 * path-independent as far as this run is concerned -- and the cache does not
 * outlive the run.
 *
 * Only ever called once ChessBoard::threeMoveRepetition() has already said yes,
 * so this locates a match rather than deciding one. It cannot manufacture a
 * repetition the engine's own test would not have reported.
 */
inline int
repetitionOwner(const ChessBoard& pos, const ProveContext& ctx, int ply)
{
  // The window bitboard.cpp uses: only a ply with the same side to move can
  // repeat, so step by 2, and nothing before the last irreversible move can.
  const int last = std::max(0, ply - pos.halfMoveClock());

  for (int p = ply - 2; p >= last; p -= 2)
    if (ctx.pathHash[p] == pos.hashValue)
      return p;

  return NO_PATH_DEP;
}

}  // namespace


/**
 * One node of the AND/OR proof.
 *
 * @param dep      out: shallowest search ply this result leans on, or
 *                 NO_PATH_DEP for none. Only meaningful when the result is
 *                 TRUE -- see the discharge rule at the bottom.
 * @param tainted  out: the result was reached with the node budget already
 *                 spent, so it means "unknown" rather than "refuted".
 * @param lastTo   destination of the move that reached this node, or SQUARE_NB
 *                 at the root. At an AND node that is the checking piece's
 *                 square, which is the key the evasion ordering sorts on.
 */
static bool
proveRec(ChessBoard& pos, ProveContext& ctx, int ply,
         PerpetualStats& st, int& dep, bool& tainted, Square lastTo)
{
  dep     = NO_PATH_DEP;
  tainted = false;

  if (ply > st.maxPly)
    st.maxPly = ply;

  // Path-based cycle detection. threeMoveRepetition() is, despite its name, a
  // 2-fold test (bitboard.cpp: `posCount >= 1`) over the undoInfo stack, which
  // is precisely the current recursion path plus the game history preceding it
  // -- so it is free to reuse and is the more correct test besides. A cycle IS
  // the thing being proven: an unending check sequence.
  //
  // Skipped at the root, where "already repeated" is the caller's question.
  if (ply > 0 and pos.threeMoveRepetition())
  {
    dep = repetitionOwner(pos, ctx, ply);
    return true;
  }

  // The clock is part of the cache key, so unlike a repetition this terminal
  // creates no dependency on the path that produced it.
  if (ply > 0 and pos.fiftyMoveDraw())
    return true;

  const int remaining = ctx.plyCap - ply;

  if (ctx.useCache)
  {
    const auto it = ctx.resolved.find(cacheKey(pos));

    if (it != ctx.resolved.end()
        and (it->second.result or remaining <= it->second.remaining))
    {
      ++st.cacheHits;
      return it->second.result;
    }
  }

  if (st.nodes >= ctx.nodeBudget or ply >= ctx.plyCap)
  {
    st.budgetHit = true;
    // Only the node budget poisons the result. Running out of PLY is recorded
    // instead in the entry's `remaining`, which is what makes those failures
    // cacheable rather than merely discarded.
    tainted = st.nodes >= ctx.nodeBudget;
    return false;                     // fail closed: "unknown", never a draw claim
  }
  ++st.nodes;
  ++st.nodesAtPly[ply];
  ctx.pathHash[ply] = pos.hashValue;

  const bool orNode = (pos.color == ctx.attacker);

  MoveList myMoves;
  stagedGenerateMoves<GEN_METADATA>(pos, myMoves);
  stagedGenerateMoves<GEN_MOVES   >(pos, myMoves);

  // GEN_CHECKS only fills data the MType::CHECK tagging below reads, and only
  // the attacker's moves are ever filtered on that tag -- at an AND node the
  // defender plays everything, so the stage would be pure cost.
  if (orNode)
    stagedGenerateMoves<GEN_CHECKS>(pos, myMoves);

  if (!myMoves.anyMove())
  {
    if (myMoves.checkers == 0)
      return true;                    // stalemate -- the draw this idea is about
    // Checkmate. Against the attacker it is a loss; against the defender the
    // attacker has done better than draw, which still satisfies the claim.
    return !orNode;
  }

  MoveArray movesArray;
  myMoves.getMoves<MType::CAPTURES | MType::QUIET, MType::CHECK>(pos, movesArray);

  if (orNode)
  {
    // Drop the non-checks up front rather than skipping them inside the loop.
    // Compaction keeps relative order, so with ordering off this is exactly the
    // sequence the old `continue` produced -- the A/B measures the sort alone.
    size_t keep = 0;
    for (size_t i = 0; i < movesArray.size(); ++i)
      if (is_type<MType::CHECK>(movesArray[i]))
        movesArray[keep++] = movesArray[i];

    while (movesArray.size() > keep)
      movesArray.popBack();

    // No early-out when nothing survives: the loop below is simply empty, which
    // already yields FALSE -- and falling through means the node still reaches
    // the cache. "Attacker has no check here" is the cheapest and most reusable
    // fact this search produces; returning early threw it away.

    if (ctx.order != CheckOrder::NONE and movesArray.size() > 1)
    {
      // Distance is measured from the moving piece's DESTINATION. For a
      // discovered check that is the wrong piece entirely and the key is
      // meaningless; those are rare enough to leave mis-sorted rather than pay
      // a per-move probe to find the real checker.
      //
      // stable_sort so that with equal distance the movegen order survives --
      // that keeps NONE a strict prefix-preserving baseline for the A/B.
      const Square emyKingSq  = squareNo(pos.getPiece(~pos.color, KING));
      const bool   farFirst   = (ctx.order == CheckOrder::FAR);

      std::stable_sort(movesArray.begin(), movesArray.end(),
        [emyKingSq, farFirst](Move a, Move b) {
          const int da = plt::manhattanDistance(to_sq(a), emyKingSq);
          const int db = plt::manhattanDistance(to_sq(b), emyKingSq);
          return farFirst ? (da > db) : (da < db);
        });
    }
  }
  else if (ctx.evasion != EvasionOrder::NONE and movesArray.size() > 1
           and lastTo != SQUARE_NB)
  {
    // A capture of the checker is distance 0, so APPROACH already ranks it
    // first and does not need the explicit key.
    const bool wantCapture = ctx.evasion == EvasionOrder::CAPTURE
                          or ctx.evasion == EvasionOrder::BOTH;
    const int  distSign    = ctx.evasion == EvasionOrder::FLEE     ?  1
                           : ctx.evasion == EvasionOrder::BOTH     ?  1
                           : ctx.evasion == EvasionOrder::APPROACH ? -1
                           :                                          0;

    std::stable_sort(movesArray.begin(), movesArray.end(),
      [lastTo, wantCapture, distSign](Move a, Move b) {
        if (wantCapture)
        {
          const bool ca = to_sq(a) == lastTo;
          const bool cb = to_sq(b) == lastTo;
          if (ca != cb) return ca;
        }
        if (distSign != 0)
        {
          const int da = plt::manhattanDistance(to_sq(a), lastTo);
          const int db = plt::manhattanDistance(to_sq(b), lastTo);
          return distSign > 0 ? (da > db) : (da < db);
        }
        return false;
      });
  }

  bool result   = !orNode;            // OR falls through to false, AND to true
  int  bestDep  = NO_PATH_DEP;
  bool anyTaint = false;

  for (const Move move : movesArray)
  {
    // At an OR node every survivor of the filter above is a check, so there is
    // nothing left to skip. If the tagger ever MISSES a check the attacker
    // simply loses a resource it had: the prover may fail to prove a draw, but
    // it can never invent one -- the safe direction for the error to run in.
    pos.makeMove(move);

    int  childDep   = NO_PATH_DEP;
    bool childTaint = false;
    const bool childOk =
      proveRec(pos, ctx, ply + 1, st, childDep, childTaint, to_sq(move));

    pos.unmakeMove();

    if (orNode ? childOk : !childOk)  // one check that holds / one escape that breaks out
    {
      // Decisive child. The conclusion rests on this one alone, so it inherits
      // that child's dependency -- and its taint -- and not the dead siblings'.
      // A sibling that ran out of budget says nothing about a conclusion that
      // does not rest on it, and letting it poison this one discards sound
      // FALSEs: precisely the AND node whose defender found a clean escape.
      result   = childOk;
      bestDep  = childDep;
      anyTaint = childTaint;

      // Name the move only at the prover's root, and only for the attacker --
      // that is the one caller (the root probe in search()) that has to PLAY
      // the proof rather than merely score it. Recording it deeper would cost
      // a store per node for something nothing reads.
      if (ply == 0 and orNode)
        st.proofMove = move;

      break;
    }

    // No decisive child yet, so the fall-through conclusion -- OR to FALSE, AND
    // to TRUE -- will rest on ALL of them, and an unknown among them is an
    // unknown in it.
    anyTaint = anyTaint or childTaint;
    bestDep  = std::min(bestDep, childDep);
  }

  tainted = anyTaint;

  // Graph-history interaction, the whole reason this is not a plain
  // transposition table. A TRUE that leaned on a repetition closing onto an
  // ancestor ABOVE this node is a draw only underneath that ancestor; a
  // different route arriving here has not earned it, and storing it is exactly
  // the false draw the prover must never produce.
  //
  // Once the owning ancestor is this node's own ply, the cycle lies entirely
  // inside this subtree and the dependency is discharged: the attacker can
  // force the return from here however here was reached. So the result becomes
  // storable, and the parent is told of no dependency at all.
  const bool pathBound = result and bestDep < ply;

  dep = pathBound ? bestDep : NO_PATH_DEP;

  if (!ctx.useCache)
    return result;

  if (pathBound)
  {
    ++st.truePathBound;
    return result;
  }

  // A FALSE reached with the node budget already gone means "unknown", not
  // "refuted", and unknowns are worth nothing to a later query.
  if (!result and anyTaint)
    return result;

  // Note the asymmetry: FALSE is stored without the graph-history gate above,
  // and that is deliberate rather than an oversight. Extra path history only
  // ever turns results TRUE -- more ancestors means more repetition hits, and a
  // repetition hit returns TRUE -- so a FALSE re-used on a richer path can only
  // ever cost a proof this run would have found. It cannot become a draw claim
  // that is not there. Losing proofs is the direction this prover is allowed to
  // err in; inventing them is not.
  const Key        key = cacheKey(pos);
  const CacheEntry fresh{result, int16_t(remaining)};

  auto it = ctx.resolved.find(key);

  if (it == ctx.resolved.end())
  {
    if (ctx.resolved.size() >= ctx.cacheCap)
      return result;                  // table full: stop growing, keep what we have

    ctx.resolved.emplace(key, fresh);
    ++st.cacheStores;
  }
  else if (!it->second.result
           and (fresh.result or fresh.remaining > it->second.remaining))
  {
    // Upgrade in place: a proof beats a failure, and a failure established with
    // more room to work in beats one established with less.
    it->second = fresh;
    ++st.cacheStores;
  }

  return result;
}


bool
provesPerpetual(ChessBoard& pos, PerpetualStats& stats,
                uint64_t nodeBudget, int plyCap, bool useCache,
                uint64_t cacheCap, CheckOrder order, EvasionOrder evasion)
{
  stats = PerpetualStats{};

  ProveContext ctx{pos.color, nodeBudget,
                   std::min(plyCap, PERPETUAL_PLY_LIMIT),
                   useCache, cacheCap, order, evasion, {}, {}};

  int  dep     = NO_PATH_DEP;
  bool tainted = false;

  const bool proven = proveRec(pos, ctx, 0, stats, dep, tainted, SQUARE_NB);

  stats.cacheEntries = ctx.resolved.size();

  return proven;
}
