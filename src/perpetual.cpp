#include <algorithm>
#include <bit>
#include <cassert>
#include <climits>

#include "perpetual.h"
#include "movegen.h"
#include "lookup_table.h"
#include "search_utils.h"


namespace {

// Dependency sentinel: this result leans on no path repetition at all.
constexpr int NO_PATH_DEP = INT_MAX;


/**
 * Sort rank for CheckOrder::HEAVY at an OR node -- the value of the piece that
 * delivers the check, heaviest first.
 *
 * Coarse on purpose. All this has to induce is the classes queen > rook >
 * minor > pawn; bishop and knight are left equal so the secondary key (or, for
 * plain HEAVY, movegen order) decides between them.
 *
 * Two moves where the key is a guess rather than a fact:
 *   - a PROMOTION checks with the piece it becomes, not with the pawn, so the
 *     promoted piece is decoded and ranked instead;
 *   - a KING move that gives check is by definition a DISCOVERED check, so the
 *     checker is some other piece entirely and the moving piece's value says
 *     nothing about it. Ranked last rather than probed for -- the same trade
 *     the distance key already makes for discoveries.
 */
int
checkerValue(Move move)
{
  PieceType pt = PieceType((move >> 12) & 7);

  // Promo piece is encoded as (type - 2) in bits 18-19: 0=B 1=N 2=R 3=Q.
  if (pt == PAWN and ((1ULL << to_sq(move)) & Rank18))
    pt = PieceType(((move >> 18) & 3) + 2);

  switch (pt)
  {
    case QUEEN:  return 9;
    case ROOK:   return 5;
    case KNIGHT: return 3;
    case BISHOP: return 3;
    case PAWN:   return 1;
    default:     return 0;  // KING -- discovered check, checker unknown
  }
}


struct ProveContext
{
  Color    attacker;
  uint64_t nodeBudget;
  int      plyCap;
  bool     useCache;
  uint64_t cacheCap;
  CheckOrder   order;
  EvasionOrder evasion;

  // Shared, grow-only, cleared by epoch bump in provesPerpetual(). See
  // PerpetualProofCache -- not owned here, which is what makes a probe
  // allocation-free.
  PerpetualProofCache& resolved;

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
 * @param mateDist out: plies to forced mate, or PERPETUAL_NO_MATE. Only
 *                 meaningful when the result is TRUE. Costs no extra nodes --
 *                 it rides the existing traversal and inherits at exactly the
 *                 sites `dep` and `tainted` do -- and therefore under-reports
 *                 wherever an OR node stopped on a drawing check before
 *                 reaching a mating one. See PerpetualStats::mateDist.
 * @param lastTo   destination of the move that reached this node, or SQUARE_NB
 *                 at the root. At an AND node that is the checking piece's
 *                 square, which is the key the evasion ordering sorts on.
 */
static bool
proveRec(ChessBoard& pos, ProveContext& ctx, int ply,
         PerpetualStats& st, int& dep, bool& tainted, int& mateDist,
         Square lastTo)
{
  dep      = NO_PATH_DEP;
  tainted  = false;
  mateDist = PERPETUAL_NO_MATE;

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
    const PerpetualProofCache::Entry* e = ctx.resolved.probe(cacheKey(pos));

    if (e != nullptr and (e->result or remaining <= e->remaining))
    {
      ++st.cacheHits;
      mateDist = e->mateDist;
      return e->result;
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
    // attacker has done better than draw, which still satisfies the claim --
    // and is the one TRUE terminal worth telling the caller apart from a draw.
    if (!orNode)
      mateDist = 0;

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
      const bool   heavyFirst = (ctx.order == CheckOrder::HEAVY
                              or ctx.order == CheckOrder::HEAVY_NEAR);
      const bool   nearFirst  = (ctx.order == CheckOrder::NEAR
                              or ctx.order == CheckOrder::HEAVY_NEAR);

      std::stable_sort(movesArray.begin(), movesArray.end(),
        [emyKingSq, farFirst, nearFirst, heavyFirst](Move a, Move b) {
          if (heavyFirst)
          {
            const int va = checkerValue(a);
            const int vb = checkerValue(b);
            if (va != vb) return va > vb;
          }
          // Plain HEAVY stops here: equal value keeps movegen order, which is
          // what makes it comparable against NONE rather than against NEAR.
          if (!nearFirst and !farFirst) return false;

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

  // For the AND-node fall-through below, where no reply broke out: this node
  // mates only if EVERY reply does, and the distance is the LONGEST of them --
  // the defender is entitled to the best defence. Unused at an OR node, which
  // either stops on a child or fails.
  bool allMate   = true;
  int  worstMate = 0;

  for (const Move move : movesArray)
  {
    // At an OR node every survivor of the filter above is a check, so there is
    // nothing left to skip. If the tagger ever MISSES a check the attacker
    // simply loses a resource it had: the prover may fail to prove a draw, but
    // it can never invent one -- the safe direction for the error to run in.
    pos.makeMove(move);

    int  childDep   = NO_PATH_DEP;
    bool childTaint = false;
    int  childMate  = PERPETUAL_NO_MATE;
    const bool childOk =
      proveRec(pos, ctx, ply + 1, st, childDep, childTaint, childMate,
               to_sq(move));

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

      // Inherited on exactly the same grounds as the two above: the conclusion
      // IS this child's. Which is also why an OR node that stopped on a
      // drawing check never learns that a later check would have mated.
      mateDist = (childOk and childMate != PERPETUAL_NO_MATE)
               ? childMate + 1 : PERPETUAL_NO_MATE;

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
    anyTaint  = anyTaint or childTaint;
    bestDep   = std::min(bestDep, childDep);
    allMate   = allMate and (childMate != PERPETUAL_NO_MATE);
    worstMate = std::max(worstMate, childMate);
  }

  // Reached only when no child was decisive, so `result` still holds its
  // fall-through value and the guards below pick out the one case that can
  // mate: an AND node every one of whose replies stayed mated. An OR node
  // failing, or an AND node with an escape, both leave `result` false.
  if (!orNode and result and allMate and movesArray.size() != 0)
    mateDist = worstMate + 1;

  tainted = anyTaint;

  // A mate is reached only through checkmate terminals -- one repeating reply
  // at an AND node erases it -- so it can lean on no repetition, and the GHI
  // machinery below must always find it free. One predictable compare per node
  // against a movegen-bound loop; cheap enough to pin the invariant with.
  assert(mateDist == PERPETUAL_NO_MATE or bestDep == NO_PATH_DEP);

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
  // Insert or upgrade in place -- a proof beats a failure, and a failure
  // established with more room to work in beats one established with less. The
  // table refuses new keys once full, keeping what it has. See store().
  if (ctx.resolved.store(cacheKey(pos), result,
                         int16_t(remaining), int16_t(mateDist)))
    ++st.cacheStores;

  return result;
}


bool
provesPerpetual(ChessBoard& pos, PerpetualStats& stats,
                uint64_t nodeBudget, int plyCap, bool useCache,
                uint64_t cacheCap, CheckOrder order, EvasionOrder evasion)
{
  stats = PerpetualStats{};

  // Sized from the run's own ceiling on distinct entries: one is stored per
  // node at most, and nodes are capped by the budget. The bump is the clear --
  // nothing from an earlier run survives it.
  perpetualProofCache.newRun(std::min(nodeBudget, cacheCap));

  // The trailing {} zeroes pathHash, which proveRec does not need -- it writes
  // ply before recursing and repetitionOwner reads only plies below the current
  // one. Left in anyway: 2 KB of memset is noise next to a run's node budget,
  // and dropping it means giving ProveContext a constructor to stop aggregate
  // initialisation from zeroing the member regardless.
  ProveContext ctx{pos.color, nodeBudget,
                   std::min(plyCap, PERPETUAL_PLY_LIMIT),
                   useCache, cacheCap, order, evasion,
                   perpetualProofCache, {}};

  int  dep      = NO_PATH_DEP;
  bool tainted  = false;
  int  mateDist = PERPETUAL_NO_MATE;

  const bool proven =
    proveRec(pos, ctx, 0, stats, dep, tainted, mateDist, SQUARE_NB);

  stats.cacheEntries = ctx.resolved.size();
  stats.mateDist     = proven ? mateDist : PERPETUAL_NO_MATE;

  return proven;
}


// The one instance, shared by every run -- see the class comment. Starts empty
// and grows on the first newRun(); search settles at 1 MiB and never resizes
// again.
PerpetualProofCache perpetualProofCache;

void
PerpetualProofCache::newRun(uint64_t wantEntries) noexcept
{
  // Twice the expected entries, so linear probing stays at or below a load
  // factor of 0.5. MAX_SLOTS is the backstop for an absurd request; the cap
  // below then keeps the table from filling past 3/4 whatever happens, which
  // is what guarantees probe() finds a stale slot and terminates.
  const size_t want = size_t(std::min<uint64_t>(wantEntries, MAX_SLOTS / 2));
  const size_t need = std::clamp(std::bit_ceil(want * 2 + 1), MIN_SLOTS, MAX_SLOTS);

  if (need > table.size())
  {
    // Resizing invalidates every index, so the old contents cannot be carried
    // across; assign() drops them and resets the stamp with them.
    table.assign(need, Entry{});
    mask  = need - 1;
    epoch = 0;
  }

  ++epoch;

  // 0 is what a freshly assigned slot holds, so it can never be a live stamp.
  // Reaching it means the counter wrapped: pay the one real clear, 1 run in
  // 65535.
  if (epoch == 0)
  {
    std::fill(table.begin(), table.end(), Entry{});
    epoch = 1;
  }

  live = 0;
  cap  = std::min<uint64_t>(wantEntries, (table.size() / 4) * 3);
}


// The one instance. 256 KB of BSS, zero-initialised, so an untouched search
// starts with every slot empty without paying for a clear.
PerpetualFailCache perpetualFailCache;

void
PerpetualFailCache::clear() noexcept
{
  table.fill(0);
}
