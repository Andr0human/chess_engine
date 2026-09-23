
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
quiescenceSearch(ChessBoard& pos,
                 Score alpha,
                 Score beta,
                 Ply ply,
                 int pvIndex)
{
  if (info.shouldStop())
    return TIMEOUT;

  // Clear the PV slot before any early return so stale moves are not copied into
  // the parent's PV.
  pvArray[pvIndex] = NULL_MOVE;

  const MoveList myMoves = generateMoves(pos);

  if (!myMoves.anyMove())
    return myMoves.checkers ? checkmateScore(ply) : VALUE_ZERO;

  // Check repetition and the 50-move rule at the qsearch entry. Deeper qsearch
  // moves are captures or promotions, so neither draw condition can arise there.
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

  // Include quiet promotions in qsearch. Capturing promotions are already included
  // in the capture list, but quiet promotions are not.
  const bool promoExists = USE_QSEARCH_PROMO and myMoves.exists<MType::PROMOTION>(pos);

  if (!myMoves.exists<MType::CAPTURES>(pos) and !promoExists)
    return alpha;

  MoveArray movesArray;
  myMoves.getMoves<MType::CAPTURES>(pos, movesArray);

  if (promoExists)
    myMoves.getMoves<MType::PROMOTION>(pos, movesArray);

  // Search at least one move at thin nodes and keep the top three otherwise, even
  // when the captures are losing.
  const size_t floor = movesArray.size() < 4 ? 1 : 3;

  // Order captures by SEE when move ordering is enabled. Otherwise search all
  // capture moves because there is no SEE cutoff.
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

// Search a child with optional LMR reduction. Re-search at full depth if the
// reduced search improves alpha.
template <bool ChildPv>
static Score
searchChild(ChessBoard& pos, const SearchContext& ctx, int R)
{
  Score eval = -alphaBeta<ChildPv>(pos, ctx.child(ctx.depth - 1 - R, ctx.alpha, ctx.beta));

  // Re-search a reduced move at full depth if it beats alpha.
  // if timed-out, eval will be highly negative thus following code won't execute
  // preventing the need to add searchStop()
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
    // The first move gets a full-window search. Later moves use a null-window scout
    // and are re-searched if they improve alpha.
    if (moveNo == 0)
    {
      eval = -alphaBeta<PvNode>(pos, ns.child(depth - 1, alpha, beta));
    }
    else
    {
      const int R = (USE_LMR and lmrOk(move, depth, moveNo)) ? reductionFunction(depth, moveNo) : 0;

      // A scout only produces a bound, so it is never searched as a PV node.
      info.pvsScouts++;
      eval = -alphaBeta<false>(pos, ns.child(depth - 1 - R, alpha, alpha + 1));

      // Re-search a move that beats alpha at full depth and with the full window.
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

    // At a PV node, only the first move is searched as a PV child when PVS is disabled.
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

// Search the hash move first at full depth so it gets the normal move-0 treatment.
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

  // The hash move is the first move, so it inherits the node's PV status.
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
playSubsetMoves(ChessBoard& pos,
                const MoveList& myMoves,
                MoveArray& movesArray,
                size_t start,
                size_t end,
                NodeState& ns,
                Move bestMove,
                bool futilityStage = false)
{
  // Account for moves searched before this stage when calculating the move number
  // used by LMR.
  const size_t moveNoBias = myMoves.removedMoves();

  for (size_t moveNo = start; moveNo < end; ++moveNo)
  {
    Move move = movesArray[moveNo];

    // Skip remaining quiet moves when the node meets the futility conditions and a
    // previous move has already provided a score.
    if (futilityStage and ns.skipsQuiets(bestMove))
      break;

    // Keep the first searched move as a fallback for a fail-low TT entry.
    if (bestMove == NULL_MOVE)
      bestMove = filter(move);

    Score eval = playMove<reduction, PvNode>(pos, move, moveNo + moveNoBias, ns);

    // Mark the node as aborted so it is not stored in the TT.
    if (info.shouldStop())
    {
      ns.aborted = true;
      return bestMove;
    }

    if (eval >= ns.beta)
    {
      ns.hashf = Flag::HASH_BETA;
      ns.alpha = ns.beta;
      bestMove = filter(move);

      if (is_type<MType::QUIET>(move))
      {
        if constexpr (USE_KILLERS)
        {
          // Killers are stored filtered, so lookups must filter too. Newest
          // killer first; the oldest is evicted when the slots are full.
          auto& killers = killerMoves[ns.ply];
          if (!killers.contains(filter(move)))
            killers.pushFront(filter(move));
        }
        if constexpr (USE_HISTORY)
        {
          updateHistory(pos.color, move, ns.depth);

          // Penalize quiet moves that were searched but failed to cause a cutoff.
          // History remains a quiet-move statistic.
          if constexpr (USE_HISTORY_MALUS)
            for (Move tried : ns.triedQuiets)
              penalizeHistory(pos.color, tried, ns.depth);
        }
      }
      break;
    }

    // Record the quiet move after the cutoff check so the winning move is not
    // penalized.
    if constexpr (USE_HISTORY and USE_HISTORY_MALUS)
      if (is_type<MType::QUIET>(move))
        ns.triedQuiets.push(move);

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

// PvNode comes first because the parameter pack must be last.
template <bool PvNode, int moveGen, MType orderType, MType... rest>
static Move
playAllMoves(ChessBoard& pos,
             MoveList& myMoves,
             MoveArray& movesArray,
             size_t start,
             NodeState& ns,
             Move bestMove)
{
  if constexpr (moveGen == 0)
    myMoves.getMoves<MType::CAPTURES>(pos, movesArray);

  if constexpr (moveGen == 1)
    myMoves.getMoves<MType::QUIET, MType::CHECK>(pos, movesArray);

  // Keep the quiet stage in orderMoves so history ordering can be applied.
  // Skip the sort when futility will immediately stop the stage.
  const bool useHistory = !(orderType == MType::QUIET and ns.skipsQuiets(bestMove));
  size_t end = orderMoves(pos, movesArray, orderType, ns.ply, start, useHistory);

  // Futility pruning is only applied to the final quiet-move stage.
  bestMove = playSubsetMoves<PvNode>(pos,
                                     myMoves,
                                     movesArray,
                                     start,
                                     end,
                                     ns,
                                     bestMove,
                                     orderType == MType::QUIET);

  // Stop processing the remaining move stages after a timeout or cutoff.
  if (ns.hashf == Flag::HASH_BETA or ns.aborted)
    return bestMove;

  if constexpr (sizeof...(rest) > 0)
    return playAllMoves<PvNode,
                        moveGen + 1,
                        rest...>(pos, myMoves, movesArray, end, ns, bestMove);

  return bestMove;
}

// Cache the static evaluation so multiple pruning heuristics can reuse it.
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
  // Build NodeState after the early exits. Depth and extension state are updated
  // in NodeState as the search progresses.
  if (info.shouldStop())
    return TIMEOUT;

  if (ctx.depth <= 0)
    return quiescenceSearch<1>(pos, ctx.alpha, ctx.beta, ctx.ply, ctx.pvIndex);

  // Clear this node's PV entry before any early return to avoid stale PV moves.
  pvArray[ctx.pvIndex] = NULL_MOVE;

  // Check repetition and the 50-move rule before probing the TT, since the TT does
  // not include the halfmove clock.
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
    Score ttValue = tt.lookupPosition(
      pos.hashValue,
      ctx.depth,
      ctx.ply,
      ctx.alpha,
      ctx.beta,
      hashMove,
      ttHit);

    info.ttProbes++;
    if (ttHit) info.ttHits++;

    // Keep searching at PV nodes even when the TT provides a usable cutoff score.
    // The hash move is still kept for move ordering.
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

  // Generate metadata and moves first. Check-generation data is deferred until
  // it is needed for move ordering.
  MoveList myMoves;
  stagedGenerateMoves<GEN_METADATA>(pos, myMoves);

  // Store the per-node search state before running pruning heuristics.
  NodeState ns{ctx};

  // Reverse futility pruning: at shallow nodes, use the static evaluation to
  // return early when it is safely above beta.
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

  // Razoring: at shallow nodes, use qsearch to verify positions whose static
  // evaluation is well below alpha.
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

  // Null-move pruning.
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
      // Search the null move with a null window and disable another null move below it.
      Score nullScore = -alphaBeta<false>(
        pos, ns.child(nullDepth, ns.beta - 1, ns.beta).withoutNull());
      pos.unmakeNullMove();

      if (info.shouldStop())
        return TIMEOUT;

      if (nullScore >= ns.beta)
      {
        // Do not propagate mate scores from a null-move search.
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

  // Set the futility flag for the quiet-move stage. The actual pruning happens
  // after at least one move has been searched.
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

  // Generate check data for move ordering.
  stagedGenerateMoves<GEN_CHECKS>(pos, myMoves);

  // Remove the already-searched hash move from the remaining move list.
  if (hashOutcome.searched)
    myMoves.removeMove(hashMove);

  // removedMoves() already accounts for the hash move, so LMR sees the correct
  // move number.

  MoveArray movesArray;
  bestMove = playAllMoves<PvNode,
                          0,
                          MType::CAPTURES,
                          MType::PROMOTION,
                          MType::CHECK,
                          MType::PV,
                          MType::KILLER,
                          MType::QUIET>(pos, myMoves, movesArray, 0, ns, bestMove);

  // Do not store a partial result from an aborted search.
  if constexpr (USE_TT) {
    if (!ns.aborted)
      tt.recordPosition(pos.hashValue, ns.depth, ns.ply, ns.alpha, ns.hashf, bestMove);
  }

  return ns.alpha;
}

// Explicit template instantiations used by this translation unit.
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

    // The root is always a PV node.
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
search(ChessBoard board,
       Depth mDepth,
       double search_time,
       std::ostream& writer,
       bool debug,
       bool emitUciInfo)
{
  resetPvLine();
  clearKillers();
  clearHistory();

  // Mark the current position so repetition detection can use the game history.
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
      // If the search stops during an iteration, keep the best completed root move.
      if (pvArray[0] != NULL_MOVE)
        info.promoteBestMove(pvArray[0]);

      if (debug and filter(info.bestMoveFound()) != filter(info.lastIterationResult().first))
        writer << "Unfinished depth " << int(depth) << " changed the best move to "
               << printMove(info.bestMoveFound(), board) << endl;
      break;
    }

    if ((eval <= alpha) or (eval >= beta))
    {
      // Widen the aspiration window and search again.
      valWindowCnt++;
      alpha = eval - (VALUE_WINDOW << valWindowCnt);
      beta  = eval + (VALUE_WINDOW << valWindowCnt);
      withinValWindow = false;
    }
    else
    {
      // Set the aspiration window for the next iteration.
      alpha = eval - VALUE_WINDOW;
      beta  = eval + VALUE_WINDOW;
      withinValWindow = true;
      valWindowCnt = 0;

      info.addResult(board, eval, pvArray, depth);
      if (debug)
        info.showLastDepthResult(board, writer);

      if (emitUciInfo)
      {
        // Build the UCI line before sending it because search and UCI I/O share the
        // output stream.
        long long timeMs = static_cast<long long>(info.timeSpent() * 1000.0);
        std::ostringstream line;
        line      << "info depth " << int(depth)
                  << " score cp " << int(eval)
                  << " nodes " << info.totalSearchedNodes()
                  << " nps " << info.nps()
                  << " time " << timeMs
                  << " pv";
        // Print the validated PV rather than the raw pvArray. Stop at the first
        // quiescence move.
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

    // Put this iteration's best move first for the next iteration.
    info.promoteBestMove(pvArray[0]);

    // Stop after finding a checkmate.
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
