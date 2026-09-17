
#include "single_thread.h"
#include "move_utils.h"
#include "node_state.h"
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

template <bool leafnode = 0>
static Score
quiescenceSearch(ChessBoard& pos, Score alpha, Score beta, Ply ply, int pvIndex)
{
  if (info.shouldStop())
    return TIMEOUT;

  // Terminate the PV here before any early return. Otherwise a no-capture
  // node leaves this slot holding a stale entry from a sibling line, which
  // addResult() copies whenever it is coincidentally legal in the new line —
  // rendering phantom captures in the printed PV (e.g. "Kd4 (Kxe7)").
  pvArray[pvIndex] = NULL_MOVE;

  const MoveList myMoves = generateMoves(pos);

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

  Score standPat = evaluate(pos);

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
    return alpha;

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

  return alpha;
}

// Full-window child search with an optional LMR reduction, used on the
// non-PVS path. ChildPv is a template parameter rather than a bool argument
// so the child's PV-ness stays compile-time all the way down; the caller
// resolves the runtime `moveNo == 0` test into one of the two instantiations.
template <bool ChildPv>
static Score
searchChild(ChessBoard& pos, const SearchContext& ctx, int R)
{
  Score eval = -alphaBeta<ChildPv>(pos, ctx.child(ctx.depth - 1 - R, ctx.alpha, ctx.beta));

  // if timed-out, eval will be highly negative thus following code won't execute
  if (R > 0 and eval > ctx.alpha)
    eval = -alphaBeta<ChildPv>(pos, ctx.child(ctx.depth - 1, ctx.alpha, ctx.beta));

  return eval;
}

template <ReductionFunc reductionFunction, bool PvNode>
static Score
playMove(ChessBoard& pos, Move move, size_t moveNo, const NodeState& ns)
{
  const Depth depth = ns.depth;
  const Score alpha = ns.alpha;
  const Score beta  = ns.beta;

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
      eval = -alphaBeta<PvNode>(pos, ns.child(depth - 1, alpha, beta));
    }
    else
    {
      const int R = (USE_LMR and lmrOk(move, depth, moveNo)) ? reductionFunction(depth, moveNo) : 0;

      // A scout is a null-window search: its score is a bound, never the real
      // thing, so it is never a PV node however this node is labelled.
      info.pvsScouts++;
      eval = -alphaBeta<false>(pos, ns.child(depth - 1 - R, alpha, alpha + 1));

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
        eval = -alphaBeta<PvNode>(pos, ns.child(depth - 1, alpha, beta));
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
        ? searchChild<true >(pos, ns, R)
        : searchChild<false>(pos, ns, R);
    }
    else
    {
      eval = searchChild<false>(pos, ns, R);
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
  Score eval = -alphaBeta<PvNode>(pos, ns.child(ns.depth - 1, ns.alpha, ns.beta));
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
  // Without this, LMR's `moveNo < LMR_LIMIT` gate would give the first staged
  // move an extra full-depth search it shouldn't get.
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

  // The residual QUIET stage is routed through orderMoves — rather than
  // short-circuiting straight to movesArray.size() — so the history sort has
  // somewhere to live, even though every prioritize/sort/killer branch inside
  // orderMoves is otherwise gated off for mTypes == MType::QUIET.
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
alphaBeta(ChessBoard& pos, SearchContext ctx)
{
  // Up to the NodeState construction below, the node's inputs are read from
  // `ctx`; from there on, only from `ns`. The split keeps the early-return
  // paths (draws, TT cutoff) from paying for NodeState's triedQuiets buffer,
  // and reading `ns` afterwards is what keeps depth pre-extension for
  // RFP / razoring / NMP and post-extension for futility / the TT store.
  if (info.shouldStop())
    return TIMEOUT;

  if (ctx.depth <= 0)
    return quiescenceSearch<1>(pos, ctx.alpha, ctx.beta, ctx.ply, ctx.pvIndex);

  // Terminate this node's PV row before any early return (same reason as in
  // quiescenceSearch). A TT cutoff / draw / RFP / razoring exit that leaves the
  // row untouched hands the parent a stale line from a sibling, which
  // addResult() then copies for as long as it happens to stay legal — printing
  // moves that were never searched here. Truncating honestly is also what lets
  // SearchData::extendPvFromTt() rebuild a *real* tail from the table.
  pvArray[ctx.pvIndex] = NULL_MOVE;

  // Repetition / 50-move draws, before the TT probe and RFP: the hash carries no
  // halfmove clock, so either could otherwise return a stale non-draw score for a
  // drawn position. Leaf nodes (depth <= 0) are handed off above and run the same
  // test inside quiescenceSearch.
  //
  // A repeated position cannot be terminal — every stored hash is one a move was
  // played from — so repetition needs no movegen. The 50-move rule does: a mate
  // delivered on the 100th half-move stands, so rule it out first. Rare enough
  // that the full movegen here costs nothing.
  if (pos.threeMoveRepetition())
    return VALUE_DRAW;

  if (pos.fiftyMoveDraw())
  {
    const MoveList drawMoves = generateMoves(pos);
    return (drawMoves.checkers and !drawMoves.anyMove()) ? checkmateScore(ctx.ply) : VALUE_DRAW;
  }

  info.addNode();

  Move hashMove = NULL_MOVE;

  if constexpr (USE_TT) {
    bool ttHit = false;
    Score ttValue = tt.lookupPosition(pos.hashValue, ctx.depth, ctx.ply, ctx.alpha, ctx.beta, hashMove, ttHit);

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

  // Per-node search state. Built here, before RFP, so the node's static eval
  // can be cached in it once (via nodeStaticEval) and reused by every
  // heuristic in the node. ns.depth / ns.numExtensions are pre-extension at
  // this point — bumped in place once the extension policy runs below.
  NodeState ns{ctx};

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
      and ns.depth <= RFP_MAX_DEPTH
      and !isMateScore(ns.beta))
    {
      const Score staticEval = nodeStaticEval(pos, ns);
      if (staticEval - RFP_MARGIN * ns.depth >= ns.beta)
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
      and ns.depth <= RAZOR_MAX_DEPTH
      and !isMateScore(ns.alpha))
    {
      const Score staticEval = nodeStaticEval(pos, ns);
      if (staticEval + RAZOR_MARGIN * ns.depth <= ns.alpha)
      {
        const Score razorScore = quiescenceSearch<1>(pos, ns.alpha, ns.beta, ns.ply, ns.pvIndex);
        if (info.shouldStop())
          return TIMEOUT;
        if (razorScore <= ns.alpha)
          return razorScore;
      }
    }
  }

  stagedGenerateMoves<GEN_MOVES   >(pos, myMoves);

  if (!myMoves.anyMove())
    return myMoves.checkers ? checkmateScore(ns.ply) : VALUE_ZERO;

  if (isTheoreticalDraw(pos))
    return VALUE_DRAW;

  // --- Null-move pruning (NMP) ---
  // Hand the opponent a free tempo and search their reply at reduced depth
  // with a null window around beta. If they still can't pull our score below
  // beta, a real move almost certainly fails high too — so prune. Uses the
  // pre-extension depth and the already-populated myMoves.checkers.
  if constexpr (USE_NMP)
  {
    if (ns.doNull
        and myMoves.checkers == 0                 // never null out of check
        and ns.depth >= NMP_MIN_DEPTH              // too shallow to be worth it
        and pos.hasNonPawnMaterial(pos.color)      // zugzwang guard
        and !isMateScore(ns.beta))                 // don't manufacture false mates
    {
      const int R = nullReduction(ns.depth);
      const int rawNullDepth = ns.depth - 1 - R;
      const Depth nullDepth = static_cast<Depth>(rawNullDepth > 0 ? rawNullDepth : 0);

      pos.makeNullMove();
      // Null-window probe around beta — never a PV node, whatever this node is.
      // No null move on the reply: two in a row just hand the tempo back.
      Score nullScore = -alphaBeta<false>(
        pos, ns.child(nullDepth, ns.beta - 1, ns.beta).withoutNull());
      pos.unmakeNullMove();

      if (info.shouldStop())
        return TIMEOUT;

      if (nullScore >= ns.beta)
      {
        // A mate score off a null move is not trustworthy — the side to move
        // was handed a free tempo. Clamp to beta rather than propagate it.
        if (isMateScore(nullScore))
          return ns.beta;
        return nullScore;
      }
    }
  }

  if constexpr (USE_EXTENSIONS) {
    int extensions = searchExtension(pos, myMoves, ns.numExtensions, ns.depth);
    ns.depth += extensions;
    ns.numExtensions += extensions;
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
      and ns.depth <= FUTILITY_MAX_DEPTH
      and !isMateScore(ns.alpha))
    {
      const Score staticEval = nodeStaticEval(pos, ns);
      ns.quietFutile = (staticEval + FUTILITY_MARGIN * ns.depth <= ns.alpha);
    }
  }

  Move bestMove = NULL_MOVE;

  HashMoveOutcome hashOutcome = playHashMove<PvNode>(pos, hashMove, ns, bestMove);
  if (hashOutcome.result.has_value())
    return *hashOutcome.result;

  // Need check-giving-square data for MType::CHECK ordering downstream.
  stagedGenerateMoves<GEN_CHECKS>(pos, myMoves);

  // Drop the already-searched hash move so subsequent getMoves<>() calls in
  // playAllMoves don't re-emit it.
  if (hashOutcome.searched)
    myMoves.removeMove(hashMove);

  // LMR bias is derived from myMoves.removedMoves() inside playSubsetMoves —
  // the removeMove() call above already bumped that counter, so the
  // LMR_LIMIT gate sees the right moveNo.

  MoveArray movesArray;
  bestMove = playAllMoves<PvNode, 0, MType::CAPTURES, MType::PROMOTION, MType::CHECK, MType::PV, MType::KILLER, MType::QUIET>
    (pos, myMoves, movesArray, 0, ns, bestMove);

  // Skip the store on an aborted node: ns.alpha is a partial bound over however
  // many moves fit in the remaining time, and writing it at full `depth` would
  // pollute the table for the next iteration *and* the next move of the game
  // (the TT is not cleared between moves).
  if constexpr (USE_TT) {
    if (!ns.aborted)
      tt.recordPosition(pos.hashValue, ns.depth, ns.ply, ns.alpha, ns.hashf, bestMove);
  }

  return ns.alpha;
}

// alphaBeta is declared in the header but only ever called from this TU, so
// the two instantiations are named here rather than exposing the definition.
template Score alphaBeta<true >(ChessBoard&, SearchContext);
template Score alphaBeta<false>(ChessBoard&, SearchContext);

Score
rootAlphaBeta(ChessBoard& pos, Score alpha, Score beta, Depth depth)
{
  int ply{0}, pvIndex{0};

  MoveArray myMoves = info.getMoves();

  pvArray[pvIndex] = NULL_MOVE;

  NodeState ns{SearchContext{alpha, beta, depth, Ply(ply), pvIndex, 0}};

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

  // Everything on the undo stack so far is game history; see threeMoveRepetition.
  board.markSearchRoot();

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
    {
      // A root move that finished before the stop and beat alpha was searched to
      // the full new depth, so it beats the move at the front. With none,
      // leave the front alone: it holds the last completed best, or a move that
      // failed high at this depth, which the NULL fallback would displace.
      if (pvArray[0] != NULL_MOVE)
        info.promoteBestMove(pvArray[0]);

      if (debug and filter(info.bestMoveFound()) != filter(info.lastIterationResult().first))
        writer << "Unfinished depth " << int(depth) << " changed the best move to "
               << printMove(info.bestMoveFound(), board) << endl;
      break;
    }

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
        // pvArray: every move in it is legality-checked, which GUIs like
        // fastchess enforce (they warn on an illegal PV move). It also
        // carries the TT-reconstructed tail, so a line ending at an
        // early-returning node still shows its full length. Stop at the
        // first quiescence move, matching the rest of the PV display.
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

    // Put this iteration's best move first for the next one. pvArray[0] is
    // NULL_MOVE on an aspiration fail-low; promoteBestMove handles that itself.
    // Before the mate break, so the front is the move to play.
    info.promoteBestMove(pvArray[0]);

    // If found a checkmate
    if (withinValWindow and isMateScore(eval)) break;
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
