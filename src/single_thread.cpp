
#include "single_thread.h"
#include "move_utils.h"
#include "node_state.h"
#include "perpetual.h"
#include "uci.h"
#include <iostream>
#include <sstream>

uint64_t
bulkCount(ChessBoard& pos, Depth depth)
{
  if (depth <= 0) return 1;

  const MoveList myMoves = generateMoves(pos);

  if (depth == 1)
    return myMoves.countMoves();

  MoveArray movesArray;
  myMoves.getMoves(pos, movesArray);

  uint64_t answer = 0;

  for (const Move move : movesArray)
  {
    pos.makeMove(move);
    answer += bulkCount(pos, depth - 1);
    pos.unmakeMove();
  }

  return answer;
}

/**
 * Tally a proof that came back as a forced mate rather than a draw.
 *
 * Called only on a proof, so `mateDist` is either PERPETUAL_NO_MATE or a real
 * distance. Clamped rather than asserted: the histogram is sized for the
 * search probe's ply cap, and a standalone caller with a looser cap should
 * land in the last bucket instead of writing off the end.
 */
static void
recordPerpetualMate(const PerpetualStats& st)
{
  if (st.mateDist == PERPETUAL_NO_MATE)
    return;

  info.perpetualMates++;
  info.perpetualMateDist[std::min(size_t(st.mateDist),
                                  info.perpetualMateDist.size() - 1)]++;
}

/**
 * The score a proof is worth at `ply` -- the whole point of carrying mateDist.
 *
 * A draw proof bounds the score from below at VALUE_DRAW. A mate proof bounds
 * it from below at "I mate in st.mateDist plies", which is the same claim made
 * sharper: the prover's distance is an UPPER bound (it measures the line found,
 * not the shortest), so the score it converts to stays a LOWER bound. Both are
 * spent the same way -- a fail-high at an interior node, an alpha clamp at the
 * root -- so this is a pure change of magnitude, not of kind.
 *
 * Negating checkmateScore() rather than open-coding VALUE_MATE - 20*ply keeps
 * the encoding in one place: the 20-points-per-ply step is not obvious, and a
 * score built with the wrong step lands outside isMateScore()'s band and reads
 * as an ordinary evaluation of about +150 pawns.
 *
 * Past MAX_PLY it does exactly that, and the prover's ply cap is independent of
 * MAX_PLY, so the case is reachable: fall back to the draw bound. Sound (a
 * forced mate is at least a draw) and still cuts, since the caller only asks
 * with beta <= VALUE_DRAW.
 */
static Score
perpetualProofScore(const PerpetualStats& st, Ply ply)
{
  if (st.mateDist == PERPETUAL_NO_MATE)
    return VALUE_DRAW;

  const Ply matePly = ply + st.mateDist;

  if (matePly > MAX_PLY)
    return VALUE_DRAW;

  return -checkmateScore(matePly);
}


/**
 * The score a FAILED probe leaves behind, given how deep it got.
 *
 * `maxPly` is the deepest ply any branch of the proof search reached, so a
 * large one says the defender was still in check when the prover ran out of
 * room. Discount the deficit; do not erase it. See PERPETUAL_RESIST_PLY_1
 * (perpetual.h) for why this is evidence rather than a bound.
 *
 * Anchored on VALUE_DRAW rather than on zero. The engine's draw is -5, not 0,
 * and the claim being made is "this is nearer a DRAW than the number says" --
 * shrinking toward zero would quietly hand back the contempt VALUE_DRAW
 * encodes. The caller only ever asks with alpha < VALUE_DRAW - PERPETUAL_MARGIN,
 * so the deficit is large and negative and the result stays strictly below
 * VALUE_DRAW: this can shrink a loss, never manufacture an advantage.
 *
 * Counts its own telemetry, so both call sites -- fresh probe and cache reuse
 * -- are covered by one increment site rather than two that can drift apart.
 */
static Score
perpetualResistanceScore(Score alpha, int maxPly)
{
  if constexpr (!USE_PERPETUAL_RESIST)
    return alpha;

  if (maxPly <= PERPETUAL_RESIST_PLY_1)
    return alpha;

  // A mate score is an encoded ply DISTANCE, not a magnitude: scaling one lands
  // it outside isMateScore()'s band, where it reads as an ordinary evaluation of
  // about 150 pawns. Nothing to discount here anyway -- the search is holding a
  // forced mate line, and "the loser can check a while first" does not make it
  // less forced, it only makes it longer.
  if (isMateScore(alpha))
    return alpha;

  const bool deep = maxPly > PERPETUAL_RESIST_PLY_2;
  const int  div  = deep ? PERPETUAL_RESIST_DIV_2 : PERPETUAL_RESIST_DIV_1;

  info.perpetualResisted++;
  info.perpetualResistedDeep += uint64_t(deep);

  return Score(int(VALUE_DRAW) + (int(alpha) - int(VALUE_DRAW)) / div);
}


/**
 * @brief The perpetual probe at a quiescence leaf. Returns alpha, possibly raised.
 *
 * Called where the side to move has exhausted its captures and is about to hand
 * a losing score back up the negamax chain. If it can force an unending check
 * sequence out of the position it is stuck with, that score is wrong by the
 * width of the whole deficit, and the proof is a hard LOWER bound of "draw".
 *
 * The spend is a plain alpha raise, not the interior probe's `>= beta` cutoff,
 * and that difference is the entire point of running here as well as there.
 * At an alphaBeta node the probe fires BEFORE the move loop, so a proof
 * short-circuits a subtree the search was in the middle of cutting anyway, and
 * seldom changes the move that ends up played. Here there is no subtree left
 * to cut: the alternative to the proof is a settled, material-down number that
 * the parent WILL consume. Replacing it changes what the parent sees by
 * construction.
 *
 * Fail-hard is not an obstacle the way it is in alphaBeta. There the raise had
 * no reachable set -- a null window cannot straddle VALUE_DRAW, so
 * `alpha < proof < beta` was unsatisfiable on every scouted node. Here the same
 * null window makes the raise a fail-HIGH instead: the node flips from
 * returning alpha ("at most this bad") to returning beta ("at least a draw"),
 * which is a different verdict propagating upward, not a discarded one. The
 * caller applies that clamp; this function only ever reports the bound.
 *
 * ---- On path dependence ----
 *
 * The prover's repetition terminal reads ChessBoard's undoInfo stack, i.e. the
 * real game history plus the search path down to this node, so a proof belongs
 * to that path and not to the position. alphaBeta's probe keeps that out of the
 * TT by returning before its store; a qsearch score cannot, because the parent
 * consumes it and stores its own.
 *
 * That leak is accepted rather than solved, on the grounds that it is not new:
 * the `threeMoveRepetition() or fiftyMoveDraw()` test at the head of
 * quiescenceSearch is path-dependent in exactly the same way and propagates the
 * same way, and has been since long before this. A perpetual proof is the same
 * class of claim -- "this path can be made to repeat" -- so it inherits that
 * standing bug rather than opening a second one. If TT path-dependence is ever
 * fixed properly, both are fixed together.
 */
static Score
qsearchPerpetualBound(ChessBoard& pos, Score alpha, Ply ply, MoveList& myMoves)
{
  // Cheapest first, the same discipline as the interior gate -- but not the
  // same ORDER, because that gate's expensive half is free here. There, "am I
  // losing badly enough for a draw to be worth proving" has to call
  // nodeStaticEval, which is why the rate limiter is placed ahead of it. Here
  // `alpha` already answers it, and answers it better: it has absorbed the
  // stand pat AND every capture line searched below this node, so it is a
  // settled score rather than a static guess. One comparison, so it leads.
  if (alpha > VALUE_DRAW - PERPETUAL_MARGIN)
    return alpha;

  // ply is the prover's base, and perpetualProofScore() rebases mateDist from
  // it. Also what keeps a long capture chain plus the prover's own ply cap
  // inside makeMove's 256-entry undoInfo stack -- the bound is 100 game plies
  // (the halfmove clock) + MAX_PLY + PERPETUAL_SEARCH_PLY_CAP.
  if (ply >= MAX_PLY)
    return alpha;

  // No queen and no rook, no check chain. One OR of two bitboards.
  if ((pos.getPiece(pos.color, QUEEN) | pos.getPiece(pos.color, ROOK)) == 0)
    return alpha;

  // The prover's share of the WHOLE search (PERPETUAL_NODE_SHARE_DIV). Shared
  // with the interior probe deliberately: the two draw on one budget, and this
  // limiter is the only thing standing between qsearch's far larger node
  // population and crowding the interior probe out of the search entirely.
  if (info.perpetualNodes > PERPETUAL_FREE_NODES
        + info.totalSearchedNodes() / PERPETUAL_NODE_SHARE_DIV)
  {
    info.perpetualThrottled++;
    return alpha;
  }

  // The vetoes, in the interior probe's order and for its reasons: the
  // open-king test is both the strongest and the cheapest, the distance test is
  // next, and the fail cache goes last because it is the only one that can be
  // wrong in a way worth avoiding.
  if (perpetualOpenKingVeto(pos))
  {
    info.perpetualOpenVetoed++;
    return alpha;
  }

  if (perpetualDistanceVeto(pos))
  {
    info.perpetualVetoed++;
    return alpha;
  }

  // Carries the earlier probe's maxPly, so a suppressed node keeps the
  // resistance discount its own probe earned instead of losing it the moment
  // the cache starts working. Untouched on a miss.
  int resistPly = 0;

  if (perpetualFailCache.failed(pos.hashValue, resistPly))
  {
    info.perpetualSuppressed++;
    return perpetualResistanceScore(alpha, resistPly);
  }

  // perpetualCaptureVeto() is deliberately NOT in this stack. It is left in
  // perpetual.cpp uncalled: it was the fourth test in the interior probe's
  // gate, and that probe has since been removed, but the test itself is the
  // one gate never tried at a leaf and is the obvious thing to reach for if
  // this stack ever needs tightening.
  //
  // It existed there to catch a node whose static eval reads lost only because
  // the eval cannot see a hanging piece -- "material is about to come back, so
  // the number that opened this gate describes a position that no longer
  // exists". That premise is already discharged here, and by a stronger
  // instrument: this node has SEARCHED its captures. If a capture were going to
  // hand the material back, alpha would have risen and the value test above
  // would have returned. Running the veto anyway would fence off precisely the
  // nodes where the capture was tried and found not to help -- SEE says the
  // material comes back, the search says it does not, and the search is right.
  //
  // The moves orderCaptures() pruned below its SEE floor do not reopen this:
  // those are the LOSING captures, and the veto only ever fires on one worth
  // PERPETUAL_CAPTURE_GAIN or more.

  // No check here, no check chain from here. Cheaper than letting the prover's
  // own root re-derive it: that path rebuilds GEN_METADATA + GEN_MOVES from
  // scratch and pays for a PerpetualStats (a 2 KB zeroed histogram) and its
  // cache, where this node already holds the metadata. Unlike alphaBeta there
  // is no `checksGenerated` flag to consult -- quiescenceSearch builds its list
  // with generateMoves(pos), which does not ask for check data.
  stagedGenerateMoves<GEN_CHECKS>(pos, myMoves);

  MoveArray checkArray;
  myMoves.getMoves<MType::CAPTURES | MType::QUIET, MType::CHECK>(pos, checkArray);

  size_t checkCount = 0;
  for (size_t i = 0; i < checkArray.size(); ++i)
    checkCount += size_t(is_type<MType::CHECK>(checkArray[i]));

  if (checkCount == 0)
    return alpha;

  PerpetualStats st;
  const bool proven = provesPerpetual(pos, st, PERPETUAL_SEARCH_NODES,
                                      PERPETUAL_SEARCH_PLY_CAP, /*useCache=*/true,
                                      PERPETUAL_MAX_CACHE,
                                      PERPETUAL_SEARCH_ORDER,
                                      PERPETUAL_SEARCH_EVASION);

  info.perpetualProbes++;
  info.perpetualNodes += st.nodes;

  if (!proven)
  {
    // Store the failure, never the proof -- the proof leans on this node's path
    // and belongs to no other, while a failure reused on a richer path is at
    // worst pessimistic. See PerpetualFailCache in perpetual.h. The depth rides
    // along so the suppressed path above can reproduce this score.
    perpetualFailCache.recordFail(pos.hashValue, st.maxPly);

    // Nothing was proven, but how far the prover got before giving up is
    // itself information about the position -- spend it as a discount.
    return perpetualResistanceScore(alpha, st.maxPly);
  }

  info.perpetualProofs++;
  recordPerpetualMate(st);

  // VALUE_DRAW for an ordinary perpetual, a mate score where the proof came
  // back as a forced mate. std::max because the proof is a LOWER bound and
  // nothing here licenses lowering a score the search already earned -- it
  // cannot bind, given the value test above, but writing the clamp is what
  // makes "lower bound" true in the code rather than only in the comment.
  return std::max(alpha, perpetualProofScore(st, ply));
}


template <bool leafnode = 0>
static Score
quiescenceSearch(ChessBoard& pos, Score alpha, Score beta, Ply ply, int pvIndex)
{
  // Check if Time Left for Search
  if (info.shouldStop())
    return TIMEOUT;

  // Terminate the PV here before any early return. Otherwise a no-capture
  // node leaves this slot holding a stale entry from a sibling line, which
  // addResult() copies whenever it is coincidentally legal in the new line —
  // rendering phantom captures in the printed PV (e.g. "Kd4 (Kxe7)").
  pvArray[pvIndex] = NULL_MOVE;

  // Not const: the perpetual probe below adds GEN_CHECKS to this list.
  MoveList myMoves = generateMoves(pos);

  if (!myMoves.anyMove())
    return myMoves.checkers ? checkmateScore(ply) : VALUE_ZERO;

  // Repetition / 50-move draws, at the leaf only — `leafnode` is set exactly on
  // the entry calls from alphaBeta, so the recursive instantiation compiles this
  // away. Without it an already-drawn leaf came back as a static eval (+3.75 on a
  // dead-drawn KRN-vs-KR at halfmove 100) and the draw only surfaced an iteration
  // later, once the same position sat at an interior node. One test here covers
  // the whole tree below: qsearch searches only captures and quiet promotions,
  // and both are irreversible and reset the halfmove clock — a promotion is a
  // pawn move — so neither draw can arise deeper. It sits *after* the
  // mate/stalemate test above because checkmate outranks the 50-move rule —
  // movegen has already run by here, so that costs nothing.
  if constexpr (leafnode)
  {
    if (pos.threeMoveRepetition() or pos.fiftyMoveDraw())
      return VALUE_DRAW;
  }

  if (isTheoreticalDraw(pos))
    return VALUE_DRAW;

  info.addQNode();

  // Get a 'Stand Pat' Score
  Score standPat = evaluate(pos);

  // Checking for beta-cutoff, usually called at the end of move-generation.
  if (standPat >= beta)
    return beta;

  // int BIG_DELTA = 925;
  // if (standPat < alpha - BIG_DELTA) return alpha;

  if (standPat > alpha) alpha = standPat;

  // A pawn one push from queening is worth ~800cp more than the static eval
  // says it is, and a CAPTURES-only move list never sees the push. Promotion-
  // *captures* carry the capture bit and so arrive with the rest, but a quiet
  // promotion is emitted under the QUIET bucket. That left every leaf holding a
  // pawn on the 7th with a clear square ahead of it evaluated as though the
  // pawn were staying a pawn -- and a leaf is exactly where the search has no
  // remaining line to discover the truth by other means.
  //
  // Not written as `if constexpr`: the runtime `and` folds away just the same
  // when the flag is off, but both branches still get type-checked, so flipping
  // USE_QSEARCH_PROMO can never fail to compile the way an untaken
  // `if constexpr` branch silently can.
  const bool promoExists = USE_QSEARCH_PROMO and myMoves.exists<MType::PROMOTION>(pos);

  if (!myMoves.exists<MType::CAPTURES>(pos) and !promoExists)
  {
    // "All captures tried" is vacuously true here -- there are none. The most
    // common quiescence node by far, and the one where a losing stand pat is
    // most plainly the final word unless something like this overturns it.
    if constexpr (USE_PERPETUAL)
      return std::min(qsearchPerpetualBound(pos, alpha, ply, myMoves), beta);

    return alpha;
  }

  MoveArray movesArray;
  myMoves.getMoves<MType::CAPTURES>(pos, movesArray);

  if (promoExists)
    myMoves.getMoves<MType::PROMOTION>(pos, movesArray);

  // Keep the single best move at thin nodes, the top 3 otherwise, even if they
  // lose material -- otherwise a node with only losing captures would collapse
  // to its stand-pat score.
  const size_t floor = movesArray.size() < 4 ? 1 : 3;

  // orderCaptures() sorts SEE-descending and hands back the prune boundary, so
  // the loop below never needs a per-move SEE check of its own. Without move
  // ordering the list is unsorted and no such boundary exists -- search it all.
  // Quiet promotions rank on the same SEE scale as the captures: seeScore()
  // credits the promoted piece minus the pawn, so an undefended queening is
  // 810 and sorts above every capture, while one that just hangs the new piece
  // falls below zero and prunes with the losing captures.
  const size_t moveCount = USE_MOVE_ORDER
    ? orderCaptures(pos, movesArray, floor) : movesArray.size();

  int pvNextIndex = pvIndex + MAX_PLY - ply;

  for (size_t moveNo = 0; moveNo < moveCount; ++moveNo)
  {
    Move qMove = movesArray[moveNo];

    pos.makeMove(qMove);
    Score score = -quiescenceSearch(pos, -beta, -alpha, ply + 1, pvNextIndex);
    pos.unmakeMove();

    if (info.shouldStop())
      return TIMEOUT;

    // Check for Beta-cutoff
    if (score >= beta) return beta;

    if (score > alpha)
    {
      alpha = score;

      if (ply < MAX_PLY)
      {
        pvArray[pvIndex] = filter(qMove) | quiescenceMove();
        movcpy (pvArray + pvIndex + 1,
                pvArray + pvNextIndex, MAX_PLY - ply - 1);
      }
    }
  }

  // Every capture searched, none of them cut: alpha is this node's settled
  // verdict and the losing side is about to be held to it. Last chance to prove
  // it can force a draw out of the position instead.
  //
  // std::min re-imposes fail-hard on the way out. The bound is a LOWER one, so
  // a raise past beta is a fail-high and beta is what the caller is owed --
  // and on a scouted node (beta == alpha + 1) that is the only shape the raise
  // can take, which is exactly how a proof here reaches the parent at all.
  if constexpr (USE_PERPETUAL)
    return std::min(qsearchPerpetualBound(pos, alpha, ply, myMoves), beta);

  return alpha;
}

// Full-window child search with an optional LMR reduction, used on the
// non-PVS path. ChildPv is a template parameter rather than a bool argument
// so the child's PV-ness stays compile-time all the way down; the caller
// resolves the runtime `moveNo == 0` test into one of the two instantiations.
template <bool ChildPv>
static Score
searchChild(
  ChessBoard& pos, Depth depth, Score alpha, Score beta,
  Ply ply, int pvNextIndex, int numExtensions, int R
)
{
  Score eval = -alphaBeta<ChildPv>(pos, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);

  // if timed-out, eval will be highly negative thus following code won't execute
  if (R > 0 and eval > alpha)
    eval = -alphaBeta<ChildPv>(pos, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);

  return eval;
}

template <ReductionFunc reductionFunction, bool PvNode>
static Score
playMove(ChessBoard& pos, Move move, size_t moveNo, const NodeState& ns)
{
  const Depth depth         = ns.depth;
  const Score alpha         = ns.alpha;
  const Score beta          = ns.beta;
  const Ply   ply           = ns.ply;
  const int   pvNextIndex   = ns.pvNextIndex();
  const int   numExtensions = ns.numExtensions;

  Score eval = VALUE_ZERO;
  pos.makeMove(move);

  if constexpr (USE_PVS)
  {
    // `moveNo == 0` is the first move searched at this node (the hash-move fast
    // path bumps moveNoBias so its successors arrive with moveNo >= 1) — i.e.
    // the PV candidate. It gets the full window; every later move is scouted
    // with a null window [alpha, alpha+1] on the assumption it's worse, and
    // only re-searched at full depth + full window if the scout beats alpha.
    if (moveNo == 0)
    {
      eval = -alphaBeta<PvNode>(pos, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
    }
    else
    {
      const int R = (USE_LMR and lmrOk(move, depth, moveNo)) ? reductionFunction(depth, moveNo) : 0;

      // A scout is a null-window search: its score is a bound, never the real
      // thing, so it is never a PV node however this node is labelled.
      info.pvsScouts++;
      eval = -alphaBeta<false>(pos, depth - 1 - R, -alpha - 1, -alpha, ply + 1, pvNextIndex, numExtensions);

      // Scout beat alpha (and timeout didn't drive it negative): re-search at
      // full depth + full window for the true score. One step undoes both the
      // null window and any LMR reduction.
      //
      // The `R > 0` term is essential and easy to miss: at a null-window node
      // (beta == alpha+1) a reduced scout that fails high gives eval == beta, so
      // `eval < beta` is false and, without `R > 0`, we'd take a beta cutoff on
      // the word of a shallow LMR-reduced scout — never verifying it at full
      // depth. That unverified-reduction cutoff is a correctness bug (it lets
      // reductions hide tactics). When R == 0 the scout is already full depth,
      // so `eval < beta` alone is the right (pure-PVS) condition.
      //
      // The re-search inherits PvNode rather than `false`: a move that beat
      // alpha at a PV node *is* on the principal variation, so this is where the
      // "first move is the PV" guess gets corrected. Without it a later move
      // taking over would leave a truncated pvArray row behind it, which is the
      // reason proper PV collection wants PVS.
      if (eval > alpha and (eval < beta or R > 0))
      {
        info.pvsResearches++;
        eval = -alphaBeta<PvNode>(pos, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
      }
    }
  }
  else
  {
    const int R = (USE_LMR and lmrOk(move, depth, moveNo)) ? reductionFunction(depth, moveNo) : 0;

    // The first move searched at a PV node is the PV candidate, so it inherits
    // PV status; its siblings are assumed worse and searched as ordinary nodes.
    // Unlike the PVS path above — where first-move-vs-scout is already a
    // structural split — the test is on the runtime `moveNo`, so it has to be
    // spelled out here to pick the right searchChild instantiation.
    if constexpr (PvNode)
    {
      eval = moveNo == 0
        ? searchChild<true >(pos, depth, alpha, beta, ply, pvNextIndex, numExtensions, R)
        : searchChild<false>(pos, depth, alpha, beta, ply, pvNextIndex, numExtensions, R);
    }
    else
    {
      eval = searchChild<false>(pos, depth, alpha, beta, ply, pvNextIndex, numExtensions, R);
    }
  }

  pos.unmakeMove();
  return eval;
}

// Searches the TT-suggested move first at the node's full (boosted) depth so
// it gets the same effective ply budget as every other move. A beta cutoff
// here returns without ever running GEN_CHECKS or orderMoves downstream.
template <bool PvNode>
static HashMoveOutcome
playHashMove(ChessBoard& pos, Move hashMove, NodeState& ns, Move& bestMove)
{
  HashMoveOutcome out;

  if (hashMove == NULL_MOVE or !isLegalMoveForPosition_V2(hashMove, pos))
    return out;

  info.hashMoveInList++;
  out.searched = true;

  const int pvNextIndex = ns.pvNextIndex();

  // The hash move is move 0 at this node, so it carries the node's PV status
  // down (same rule as playMove's `moveNo == 0`).
  pos.makeMove(hashMove);
  Score eval = -alphaBeta<PvNode>(pos, ns.depth - 1, -ns.beta, -ns.alpha, ns.ply + 1, pvNextIndex, ns.numExtensions);
  pos.unmakeMove();

  if (info.shouldStop())
  {
    out.result = TIMEOUT;
    return out;
  }

  if (eval >= ns.beta)
  {
    info.hashMoveCutoffs++;
    if constexpr (USE_TT)
      tt.recordPosition(pos.hashValue, ns.depth, ns.ply, ns.beta, Flag::HASH_BETA, filter(hashMove));
    out.result = ns.beta;
    return out;
  }

  bestMove = filter(hashMove);
  if (eval > ns.alpha)
  {
    ns.hashf = Flag::HASH_EXACT;
    ns.alpha = eval;
    pvArray[ns.pvIndex] = filter(hashMove);
    movcpy(pvArray + ns.pvIndex + 1, pvArray + pvNextIndex, MAX_PLY - ns.ply - 1);
  }

  return out;
}

template <bool PvNode>
static Move
playSubsetMoves(
  ChessBoard& pos, const MoveList& myMoves, MoveArray& movesArray,
  size_t start, size_t end,
  NodeState& ns, Move bestMove, bool futilityStage = false
)
{
  // myMoves.removedMoves() accounts for moves searched *outside* this array
  // (the hash-move fast-path searches 1 move before playAllMoves runs).
  // Without this, LMR's `moveNo < LMR_LIMIT` gate gives the first staged
  // move an extra full-depth search the old impl wouldn't have done —
  // that's where the ~25% tree-bloat was coming from.
  const size_t moveNoBias = myMoves.removedMoves();

  for (size_t moveNo = start; moveNo < end; ++moveNo)
  {
    Move move = movesArray[moveNo];

    // Quiet-move futility: at a shallow, not-in-check node whose static eval
    // sits a depth-scaled margin below alpha (ns.quietFutile, computed in
    // alphaBeta), the residual quiet moves are very unlikely to raise it. Once
    // at least one move has been searched (bestMove set — so the fail-low
    // return is backed by a real score), skip the rest. This is reached with
    // futilityStage == true only for the QUIET residual (captures / promotions
    // / checks / PV / killers ran in earlier stages), so every move here is
    // already quiet & non-check — no per-move type test needed. Unverified bet
    // (cf. razoring's qsearch check); the depth-scaled margin is the safety.
    if (futilityStage and ns.skipsQuiets(bestMove))
      break;

    // HASH_ALPHA fallback: remember the first searched move at this node so
    // the TT entry has *something* to suggest on a fail-low revisit.
    if (bestMove == NULL_MOVE)
      bestMove = filter(move);

    Score eval = playMove<reduction, PvNode>(pos, move, moveNo + moveNoBias, ns);

    // No time left! Flag the node as aborted so the caller skips the TT store.
    if (info.shouldStop())
    {
      ns.aborted = true;
      return bestMove;
    }

    //! TODO: Why beta is not in root-search??
    // beta-cut found
    if (eval >= ns.beta)
    {
      ns.hashf = Flag::HASH_BETA;
      ns.alpha = ns.beta;
      bestMove = filter(move);

      if (is_type<MType::QUIET>(move))
      {
        killerMoves[ns.ply].addKillerMove(move);
        if constexpr (USE_HISTORY)
        {
          updateHistory(pos.color, move, ns.depth);

          // Malus: every quiet that was searched at this node and failed to cut
          // off gets pushed down by the same depth weight that lifts the winner.
          // Kept inside the QUIET guard on purpose -- a capture cutoff leaves
          // the table untouched on both the reward and the penalty side, so
          // history stays a quiet-only statistic.
          if constexpr (USE_HISTORY_MALUS)
            for (Move tried : ns.triedQuiets)
              penalizeHistory(pos.color, tried, ns.depth);
        }
      }
      break;
    }

    // Register the failure. Placed *after* the cutoff break, so the winner is
    // never in its own penalty list, and *after* the shouldStop() check above,
    // because a timed-out move's score is garbage -- it did not really fail.
    if constexpr (USE_HISTORY and USE_HISTORY_MALUS)
      if (is_type<MType::QUIET>(move))
        ns.triedQuiets.add(move);

    // Better move found, update the result
    if (eval > ns.alpha) {
      ns.hashf = Flag::HASH_EXACT;
      ns.alpha = eval;
      bestMove = filter(move);
      pvArray[ns.pvIndex] = filter(move);
      movcpy (pvArray + ns.pvIndex + 1,
              pvArray + ns.pvNextIndex(), MAX_PLY - ns.ply - 1);
    }
  }

  return bestMove;
}

// PvNode leads the parameter list because `rest` is a pack and must come last.
template <bool PvNode, int moveGen, MType orderType, MType... rest>
static Move
playAllMoves(
  ChessBoard& pos, MoveList& myMoves,
  MoveArray movesArray, size_t start,
  NodeState& ns, Move bestMove
)
{
  if constexpr (moveGen == 0)
    myMoves.getMoves<MType::CAPTURES>(pos, movesArray);

  if constexpr (moveGen == 1)
    myMoves.getMoves<MType::QUIET, MType::CHECK>(pos, movesArray);

  // The residual QUIET stage used to short-circuit to movesArray.size() here.
  // That was only ever an optimization: with mTypes == MType::QUIET every
  // prioritize/sort/killer branch inside orderMoves is gated off and it returns
  // movesArray.size() anyway. Routing the stage through orderMoves gives the
  // history sort somewhere to live.
  //
  // Don't pay for that sort when playSubsetMoves is about to break on move 0 --
  // literally its own break condition (ns.skipsQuiets), evaluated one call
  // earlier on the same unchanged state, and true on a large minority of
  // shallow quiet stages. The stage test mirrors the `futilityStage` argument
  // below, so the two predicates stay identical by construction.
  const bool useHistory = !(orderType == MType::QUIET and ns.skipsQuiets(bestMove));
  size_t end = orderMoves(pos, movesArray, orderType, ns.ply, start, useHistory);

  // Only the residual QUIET stage may futility-prune; earlier stages (captures,
  // promotions, checks, PV, killers) always search their moves.
  bestMove = playSubsetMoves<PvNode>(pos, myMoves, movesArray, start, end, ns, bestMove,
                                     orderType == MType::QUIET);

  // Out of time — don't walk the remaining stages just to abort in each of them.
  if (ns.hashf == Flag::HASH_BETA or ns.aborted)
    return bestMove;

  if constexpr (sizeof...(rest) > 0)
    return playAllMoves<PvNode, moveGen + 1, rest...>(pos, myMoves, movesArray, end, ns, bestMove);

  return bestMove;
}

// Lazily compute and cache the node's static evaluation. Multiple search
// heuristics (RFP today; razoring / futility / improving later) want the same
// value — compute it at most once per node and reuse it from NodeState.
static inline Score
nodeStaticEval(ChessBoard& pos, NodeState& ns)
{
  if (!ns.staticEval.has_value())
    ns.staticEval = evaluate(pos);
  return *ns.staticEval;
}

template <bool PvNode>
Score
alphaBeta(ChessBoard& pos, Depth depth, Score alpha, Score beta, Ply ply, int pvIndex, int numExtensions, bool doNull)
{
  if (info.shouldStop())
    return TIMEOUT;

    // Depth 0, starting Quiensense Search
  if (depth <= 0)
    return quiescenceSearch<1>(pos, alpha, beta, ply, pvIndex);

  // Terminate this node's PV row before any early return (same reason as in
  // quiescenceSearch). A TT cutoff / draw / RFP / razoring exit that leaves the
  // row untouched hands the parent a stale line from a sibling, which
  // addResult() then copies for as long as it happens to stay legal — printing
  // moves that were never searched here. Truncating honestly is also what lets
  // SearchData::extendPvFromTt() rebuild a *real* tail from the table.
  pvArray[pvIndex] = NULL_MOVE;

  // Cheap repetition / 50-move draws — no movegen needed. Leaf nodes (depth <= 0)
  // are handed off above and run the same test inside quiescenceSearch, where it
  // sits after move generation so mate takes precedence over the 50-move rule.
  if (pos.threeMoveRepetition() or pos.fiftyMoveDraw())
    return VALUE_DRAW;

  info.addNode();

  Move hashMove = NULL_MOVE;

  if constexpr (USE_TT) {
    bool ttHit = false;
    Score ttValue = tt.lookupPosition(pos.hashValue, depth, ply, alpha, beta, hashMove, ttHit);

    info.ttProbes++;
    if (ttHit) info.ttHits++;

    // A PV node declines the cutoff and searches on. Returning a bare score
    // here leaves this node's pvArray row NULL_MOVE, and the whole line below
    // it is lost to the display — the measured cause of a 7-ply PV printed on
    // a 37-ply search. The hash move is kept
    // either way: lookupPosition() surfaces it before the depth test, so
    // ordering still gets its best-move hint and only the *early return* is
    // given up. Non-PV nodes are untouched, so the cost is bounded to one node
    // per ply on the leftmost path.
    if (ttValue != VALUE_UNKNOWN) {
      if constexpr (!PvNode) {
        info.ttCutoffs++;
        return ttValue;
      }
      else info.pvTtCutoffsDeclined++;
    }

    if (hashMove != NULL_MOVE)
      info.ttMoveProvided++;
  }

  // Movegen: GEN_METADATA + GEN_MOVES first so the extension policy
  // (countMoves), terminal/draw checks, and theoretical-draw recognizer all
  // see complete data. GEN_CHECKS is deferred — it's only needed for
  // MType::CHECK ordering inside playAllMoves, so a hash-move beta cutoff
  // skips it (and all of orderMoves/staging) entirely.
  MoveList myMoves;
  stagedGenerateMoves<GEN_METADATA>(pos, myMoves);

  // GEN_CHECKS is normally deferred to just above playAllMoves, but the
  // perpetual probe needs it earlier. Whoever gets there first runs it.
  bool checksGenerated = false;

  // Per-node search state. Built here, before RFP, so the node's static eval
  // can be cached in it once (via nodeStaticEval) and reused by every
  // heuristic in the node. depth / numExtensions are pre-extension at this
  // point — synced into ns after the extension policy runs below.
  NodeState ns{alpha, beta, depth, ply, pvIndex, numExtensions};

  // Reverse futility pruning: at a shallow, not-in-check node, if the static
  // eval already beats beta by a depth-scaled margin, assume some move holds
  // the cutoff and return without generating/searching moves. nodeStaticEval()
  // is computed lazily — only when the cheap gates (not in check, shallow
  // depth, non-mate window) pass. Fail-soft return: staticEval (>= beta, since
  // staticEval - margin*depth >= beta) hands the parent a truer lower bound
  // than a flat beta. No TT store on the prune; bestMove untouched, so the
  // fail-low TT-hint/LMR gotcha (best-move semantics) does not apply here.
  if constexpr (USE_RFP)
  {
    if (myMoves.checkers == 0
      and depth <= RFP_MAX_DEPTH
      and !isMateScore(beta))
    {
      const Score staticEval = nodeStaticEval(pos, ns);
      if (staticEval - RFP_MARGIN * depth >= beta)
        return staticEval;
    }
  }

  // Razoring: alpha-side mirror of RFP. At a shallow, not-in-check node whose
  // static eval sits a depth-scaled margin *below* alpha, the node looks
  // hopeless on the alpha side. Rather than trust the static eval blindly,
  // verify with a quiescence search (it resolves hanging captures the static
  // eval missed); only if qsearch still fails low (<= alpha) do we return that
  // fail-soft score instead of a full-width search. If qsearch beats alpha the
  // node wasn't hopeless after all — fall through. Eval reuses the RFP cache
  // (nodeStaticEval computes at most once per node). Like RFP: no TT store,
  // bestMove untouched, so the fail-low TT-hint/LMR gotcha does not apply.
  if constexpr (USE_RAZOR)
  {
    if (myMoves.checkers == 0
      and depth <= RAZOR_MAX_DEPTH
      and !isMateScore(alpha))
    {
      const Score staticEval = nodeStaticEval(pos, ns);
      if (staticEval + RAZOR_MARGIN * depth <= alpha)
      {
        const Score razorScore = quiescenceSearch<1>(pos, alpha, beta, ply, pvIndex);
        if (info.shouldStop())
          return TIMEOUT;
        if (razorScore <= alpha)
          return razorScore;
      }
    }
  }

  stagedGenerateMoves<GEN_MOVES   >(pos, myMoves);

  if (!myMoves.anyMove())
    return myMoves.checkers ? checkmateScore(ply) : VALUE_ZERO;

  if (isTheoreticalDraw(pos))
    return VALUE_DRAW;

  // --- Null-move pruning (NMP) ---
  // Hand the opponent a free tempo and search their reply at reduced depth
  // with a null window around beta. If they still can't pull our score below
  // beta, a real move almost certainly fails high too — so prune. Uses the
  // pre-extension depth and the already-populated myMoves.checkers.
  if constexpr (USE_NMP)
  {
    if (doNull
        and myMoves.checkers == 0                 // never null out of check
        and depth >= NMP_MIN_DEPTH                 // too shallow to be worth it
        and pos.hasNonPawnMaterial(pos.color)      // zugzwang guard
        and !isMateScore(beta))                    // don't manufacture false mates
    {
      const int R = nullReduction(depth);
      const int rawNullDepth = depth - 1 - R;
      const Depth nullDepth = static_cast<Depth>(rawNullDepth > 0 ? rawNullDepth : 0);
      const int pvNextIndex = pvIndex + MAX_PLY - ply;

      pos.makeNullMove();
      // Null-window probe around beta — never a PV node, whatever this node is.
      Score nullScore = -alphaBeta<false>(pos, nullDepth, -beta, -beta + 1,
                                          ply + 1, pvNextIndex, numExtensions,
                                          /*doNull=*/false);
      pos.unmakeNullMove();

      if (info.shouldStop())
        return TIMEOUT;

      if (nullScore >= beta)
      {
        // A mate score off a null move is not trustworthy — the side to move
        // was handed a free tempo. Clamp to beta rather than propagate it.
        if (isMateScore(nullScore))
          return beta;
        return nullScore;
      }
    }
  }

  if constexpr (USE_EXTENSIONS) {
    int extensions = searchExtension(pos, myMoves, numExtensions, depth);
    depth += extensions;
    numExtensions += extensions;
    ns.depth = depth;
    ns.numExtensions = numExtensions;
  }

  // Quiet-move futility precondition (consumed in the QUIET stage of
  // playAllMoves -> playSubsetMoves). At a shallow, not-in-check node outside
  // the mate window whose static eval sits a depth-scaled margin below alpha,
  // the residual quiet moves are very unlikely to raise it. We only *flag* it
  // here; the actual skip happens per-move, after >=1 move is searched, so the
  // fail-low return stays backed by a real score. depth is post-extension
  // (an extended = interesting node gets a larger depth/margin -> futility
  // fires less). nodeStaticEval reuses the RFP/razoring cache, so this adds no
  // eval cost at depth <= RFP_MAX_DEPTH (RFP already computed it for non-check
  // nodes). No TT/bestMove effect — like RFP/razoring, the LMR gotcha is N/A.
  if constexpr (USE_FUTILITY)
  {
    if (myMoves.checkers == 0
      and depth <= FUTILITY_MAX_DEPTH
      and !isMateScore(alpha))
    {
      const Score staticEval = nodeStaticEval(pos, ns);
      ns.quietFutile = (staticEval + FUTILITY_MARGIN * depth <= alpha);
    }
  }

  Move bestMove = NULL_MOVE;

  HashMoveOutcome hashOutcome = playHashMove<PvNode>(pos, hashMove, ns, bestMove);
  if (hashOutcome.result.has_value())
    return *hashOutcome.result;

  // Need check-giving-square data for MType::CHECK ordering downstream.
  if (!checksGenerated)
  {
    stagedGenerateMoves<GEN_CHECKS>(pos, myMoves);
    checksGenerated = true;
  }

  // Drop the already-searched hash move so subsequent getMoves<>() calls in
  // playAllMoves don't re-emit it. Both branches now feed the same uniform
  // playAllMoves<0, …> entry.
  if (hashOutcome.searched)
    myMoves.removeMove(hashMove);

  // LMR bias is now derived from myMoves.removedMoves() inside
  // playSubsetMoves — the hash-move fast-path's removeMove() call already
  // bumped that counter, so the LMR_LIMIT gate sees the right moveNo.

  MoveArray movesArray;
  bestMove = playAllMoves<PvNode, 0, MType::CAPTURES, MType::PROMOTION, MType::CHECK, MType::PV, MType::KILLER, MType::QUIET>
    (pos, myMoves, movesArray, 0, ns, bestMove);

  // Skip the store on an aborted node: ns.alpha is a partial bound over however
  // many moves fit in the remaining time, and writing it at full `depth` would
  // pollute the table for the next iteration *and* the next move of the game
  // (the TT is not cleared between moves).
  if constexpr (USE_TT) {
    if (!ns.aborted)
      tt.recordPosition(pos.hashValue, depth, ply, ns.alpha, ns.hashf, bestMove);
  }

  return ns.alpha;
}

// alphaBeta is declared in the header but only ever called from this TU, so
// the two instantiations are named here rather than exposing the definition.
template Score alphaBeta<true >(ChessBoard&, Depth, Score, Score, Ply, int, int, bool);
template Score alphaBeta<false>(ChessBoard&, Depth, Score, Score, Ply, int, int, bool);

Score
rootAlphaBeta(ChessBoard& pos, Score alpha, Score beta, Depth depth)
{
  int ply{0}, pvIndex{0};

  MoveArray myMoves = info.getMoves();

  pvArray[pvIndex] = NULL_MOVE; // no pv yet

  NodeState ns{alpha, beta, depth, Ply(ply), pvIndex, 0};

  for (size_t moveNo = 0; moveNo < myMoves.size(); ++moveNo)
  {
    Move move = myMoves[moveNo];

    // The root is a PV node by definition — every PV node below it is reached
    // by taking first moves from here down.
    Score eval = playMove<rootReduction, true>(pos, move, moveNo, ns);

    info.insertMoveToList(moveNo);

    if (info.shouldStop())
      return TIMEOUT;

    if (eval > ns.alpha)
    {
      ns.alpha = eval;
      pvArray[pvIndex] = filter(move);
      movcpy (pvArray + pvIndex + 1, pvArray + ns.pvNextIndex(), MAX_PLY - ply - 1);
    }
  }

  return ns.alpha;
}

void
search(ChessBoard board, Depth mDepth, double search_time, std::ostream& writer, bool debug, bool emitUciInfo)
{
  resetPvLine();
  clearKillers();
  clearHistory();
  perpetualFailCache.clear();

  if (!generateMoves(board).anyMove())
  {
    writer << "Position has no legal moves! Discarding Search." << endl;
    return;
  }

  info = SearchData(board, search_time);

  bool withinValWindow = true;
  Score alpha = -VALUE_INF, beta = VALUE_INF;
  int valWindowCnt = 0;

  if (debug)
    info.showHeader(writer);

  for (Depth depth = 1; depth <= mDepth;)
  {
    Score eval = rootAlphaBeta(board, alpha, beta, depth);

    if (info.shouldStop())
      break;

    if ((eval <= alpha) or (eval >= beta))
    {
      // We fell outside the window, so try again with a wider Window
      valWindowCnt++;
      alpha = eval - (VALUE_WINDOW << valWindowCnt);
      beta  = eval + (VALUE_WINDOW << valWindowCnt);
      withinValWindow = false;
    }
    else
    {
      // Set up the window for the next iteration.
      alpha = eval - VALUE_WINDOW;
      beta  = eval + VALUE_WINDOW;
      withinValWindow = true;
      valWindowCnt = 0;

      info.addResult(board, eval, pvArray, depth);
      if (debug)
        info.showLastDepthResult(board, writer);

      if (emitUciInfo)
      {
        // Built into a string and handed to uciSend() rather than streamed
        // straight to std::cout: this runs on the search worker while the UCI
        // loop may be answering `isready` on the main thread, and the two share
        // one unsynchronised streambuf (see uciSend in uci.h).
        long long timeMs = static_cast<long long>(info.timeSpent() * 1000.0);
        std::ostringstream line;
        line      << "info depth " << int(depth)
                  << " score cp " << int(eval)
                  << " nodes " << info.totalSearchedNodes()
                  << " nps " << info.nps()
                  << " time " << timeMs
                  << " pv";
        // Print the validated PV (built by addResult above), not the raw
        // pvArray: every move in it is legality-checked, where a raw walk used
        // to emit illegal moves (fastchess "Illegal PV move" warnings). It also
        // carries the TT-reconstructed tail, so a line ending at an
        // early-returning node still shows its full length. Stop at the first
        // quiescence move, as the prior raw printer did.
        for (const Move m : info.getPvLine())
        {
          if (m & quiescenceMove()) break;
          line << " " << moveToUci(m);
        }
        uciSend(line.str());
      }

      info.resetNodeCount();

      depth++;
    }

    // If found a checkmate
    if (withinValWindow and (__abs(eval) >= VALUE_INF - 500)) break;

    // Put this iteration's best move first for the next one. pvArray[0] is
    // NULL_MOVE on an aspiration fail-low; promoteBestMove handles that itself.
    info.promoteBestMove(pvArray[0]);
  }

  info.searchCompleted();

  if (debug)
  {
    double hitRate = info.ttProbes
      ? 100.0 * double(info.ttHits) / double(info.ttProbes) : 0.0;
    double ttCutRate = info.ttHits
      ? 100.0 * double(info.ttCutoffs) / double(info.ttHits) : 0.0;
    writer << "TT: probes=" << info.ttProbes
           << " hits=" << info.ttHits << " (" << std::fixed << std::setprecision(1) << hitRate << "%)"
           << " cutoffs=" << info.ttCutoffs << " (" << ttCutRate << "% of hits)" << endl;

    double cutoffRate = info.hashMoveInList
      ? 100.0 * double(info.hashMoveCutoffs) / double(info.hashMoveInList) : 0.0;
    double availRate = info.ttMoveProvided
      ? 100.0 * double(info.hashMoveInList) / double(info.ttMoveProvided) : 0.0;
    writer << "Hash move: ttProvided=" << info.ttMoveProvided
           << " inList=" << info.hashMoveInList << " (" << std::fixed << std::setprecision(1) << availRate << "%)"
           << " cutoffs=" << info.hashMoveCutoffs << " (" << cutoffRate << "% of inList)" << endl;

    writer << "PV nodes: ttCutoffsDeclined=" << info.pvTtCutoffsDeclined << endl;

    if constexpr (USE_PERPETUAL)
    {
      double proofRate = info.perpetualProbes
        ? 100.0 * double(info.perpetualProofs) / double(info.perpetualProbes) : 0.0;
      double nodeShare = info.totalSearchedNodes()
        ? 100.0 * double(info.perpetualNodes) / double(info.totalSearchedNodes()) : 0.0;
      // suppressed/(suppressed+probes) is the re-probe rate the fail cache
      // collapses -- near zero means the cost was distinct positions all along.
      const uint64_t asked = info.perpetualProbes + info.perpetualSuppressed;
      double suppressRate = asked
        ? 100.0 * double(info.perpetualSuppressed) / double(asked) : 0.0;

      writer << "Perpetual: probes=" << info.perpetualProbes
             << " suppressed=" << info.perpetualSuppressed
             << " (" << std::fixed << std::setprecision(1) << suppressRate << "%)"
             << " throttled=" << info.perpetualThrottled
             << " openVeto=" << info.perpetualOpenVetoed
             << " vetoed=" << info.perpetualVetoed
             << " proofs=" << info.perpetualProofs
             << " (" << std::fixed << std::setprecision(1) << proofRate << "%)"
             << " proverNodes=" << info.perpetualNodes
             << " (" << nodeShare << "% of search nodes)" << endl;

      // The frequency question the mate work is gated on. A floor, twice over:
      // proofs the OR short-circuit never looked past, and cache hits that
      // replayed a TRUE stored without a mate. See SearchData::perpetualMates.
      double mateRate = info.perpetualProofs
        ? 100.0 * double(info.perpetualMates) / double(info.perpetualProofs) : 0.0;

      writer << "Perpetual: mates=" << info.perpetualMates
             << " (" << std::fixed << std::setprecision(1) << mateRate
             << "% of proofs)";

      if (info.perpetualMates != 0)
      {
        writer << " dist=";
        for (size_t d = 0; d < info.perpetualMateDist.size(); ++d)
          if (info.perpetualMateDist[d] != 0)
            writer << ' ' << d << ':' << info.perpetualMateDist[d];
      }
      writer << endl;

      if constexpr (USE_PERPETUAL_RESIST)
      {
        // Read `resist` against probes + suppressed, not against probes: a
        // cached failure hands the discount out again without re-probing.
        const uint64_t served = info.perpetualProbes + info.perpetualSuppressed;
        double resistRate = served
          ? 100.0 * double(info.perpetualResisted) / double(served) : 0.0;

        writer << "Perpetual: resist=" << info.perpetualResisted
               << " (" << std::fixed << std::setprecision(1) << resistRate
               << "% of scores served)"
               << " deep=" << info.perpetualResistedDeep << endl;
      }
    }

    if constexpr (USE_PVS)
    {
      double researchRate = info.pvsScouts
        ? 100.0 * double(info.pvsResearches) / double(info.pvsScouts) : 0.0;
      writer << "PVS: scouts=" << info.pvsScouts
             << " researches=" << info.pvsResearches
             << " (" << std::fixed << std::setprecision(1) << researchRate << "% of scouts)" << endl;
    }

    writer << "Nodes: " << info.totalSearchedNodes()
           << " | Time: " << std::fixed << std::setprecision(2) << info.timeSpent() << "s"
           << " | NPS: " << info.nps() << endl;
    writer << "Search Done!" << endl;
  }
}
