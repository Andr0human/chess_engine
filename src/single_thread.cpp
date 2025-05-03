
#include "single_thread.h"

uint64_t
bulkCount(ChessBoard& pos, Depth depth)
{
  if (depth <= 0) return 1;

  const MoveList myMoves = generateMoves(pos, true);

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
  // Check if Time Left for Search
  if (info.timeOver())
    return TIMEOUT;

  const MoveList myMoves = generateMoves(pos);

  if (myMoves.countMoves() == 0)
    return myMoves.checkers ? checkmateScore(ply) : VALUE_ZERO;

  if (!myMoves.exists<MType::CAPTURES>(pos) and isTheoreticalDraw(pos))
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

  if (!myMoves.exists<MType::CAPTURES>(pos))
    return alpha;

  MoveArray movesArray;
  myMoves.getMoves<MType::CAPTURES>(pos, movesArray);

  if constexpr (USE_MOVE_ORDER)
    orderMoves(pos, movesArray, MType::CAPTURES, 0);

  pvArray[pvIndex] = 0; // no pv yet
  int pvNextIndex = pvIndex + MAX_PLY - ply;

  const size_t LMP_THRESHOLD = movesArray.size() < 4 ? 1 : 3;

  for (size_t moveNo = 0; moveNo < movesArray.size(); ++moveNo)
  {
    Move captureMove = movesArray[moveNo];
    if (moveNo >= LMP_THRESHOLD and seeScore(pos, captureMove) < 0)
      continue;

    pos.makeMove(captureMove);
    Score score = -quiescenceSearch(pos, -beta, -alpha, ply + 1, pvNextIndex);
    pos.unmakeMove();

    if (info.timeOver())
      return TIMEOUT;

    // Check for Beta-cutoff
    if (score >= beta) return beta;

    if (score > alpha)
    {
      alpha = score;

      if (ply < MAX_PLY)
      {
        pvArray[pvIndex] = filter(captureMove) | quiescenceMove();
        movcpy (pvArray + pvIndex + 1,
                pvArray + pvNextIndex, MAX_PLY - ply - 1);
      }
    }
  }

  return alpha;
}

template <ReductionFunc reductionFunction>
static Score
playMove(ChessBoard& pos, Move move, size_t moveNo,
  Depth depth, Score alpha, Score beta, Ply ply, int pvNextIndex, int numExtensions)
{
  const int R = USE_LMR and lmrOk(move, depth, moveNo) ? reductionFunction(depth, moveNo) : 0;

  pos.makeMove(move);
  Score eval = -alphaBeta(pos, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
  pos.unmakeMove();

  // if timed-out, eval will be highly negative thus following code won't execute
  if ((eval > alpha) and (R > 0))
  {
    pos.makeMove(move);
    eval = -alphaBeta(pos, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
    pos.unmakeMove();
  }

  return eval;
}

static void
playSubsetMoves(
  ChessBoard& pos, MoveArray& movesArray,
  size_t start, size_t end,
  Score& alpha, Score& beta,
  Depth depth, Ply ply,
  int pvIndex, int numExtensions,
  Flag& hashf
)
{
  int pvNextIndex = pvIndex + MAX_PLY - ply;

  for (size_t moveNo = start; moveNo < end; ++moveNo)
  {
    Move move = movesArray[moveNo];
    Score eval = playMove<reduction>(pos, move, moveNo, depth, alpha, beta, ply, pvNextIndex, numExtensions);

    // No time left!
    if (info.timeOver())
      return;

    //! TODO: Why beta is not in root-search??
    // beta-cut found
    if (eval >= beta)
    {
      hashf = Flag::HASH_BETA;
      alpha = beta;

      if (is_type<MType::QUIET>(move))
        killerMoves[ply].addKillerMove(move);
      break;
    }

    // Better move found, update the result
    if (eval > alpha) {
      hashf = Flag::HASH_EXACT, alpha = eval;
      pvArray[pvIndex] = filter(move);
      movcpy (pvArray + pvIndex + 1,
              pvArray + pvNextIndex, MAX_PLY - ply - 1);
    }
  }
}

template <int moveGen, MType orderType, MType... rest>
static void
playAllMoves(
  ChessBoard& pos, MoveList& myMoves,
  MoveArray movesArray, size_t start,
  Score& alpha, Score& beta,
  Depth depth, Ply ply,
  int pvIndex, int numExtensions,
  Flag& hashf
)
{
  // Set pvArray, for storing the search_tree
  if constexpr (moveGen == 0)
  {
    pvArray[pvIndex] = NULL_MOVE;
    myMoves.getMoves<MType::CAPTURES>(pos, movesArray);
  }

  if constexpr (moveGen == 1)
    myMoves.getMoves<MType::QUIET, MType::CHECK>(pos, movesArray);

  size_t end = orderType == MType::QUIET ? movesArray.size() : orderMoves(pos, movesArray, orderType, ply, start);
  playSubsetMoves(pos, movesArray, start, end, alpha, beta, depth, ply, pvIndex, numExtensions, hashf);

  if (hashf == Flag::HASH_BETA)
    return;

  if constexpr (sizeof...(rest) > 0)
    playAllMoves<moveGen + 1, rest...>
      (pos, myMoves, movesArray, end, alpha, beta, depth, ply, pvIndex, numExtensions, hashf);
}

Score
alphaBeta(ChessBoard& pos, Depth depth, Score alpha, Score beta, Ply ply, int pvIndex, int numExtensions)
{
  if (info.timeOver())
    return TIMEOUT;

    // Depth 0, starting Quiensense Search
  if (depth <= 0)
    return quiescenceSearch<1>(pos, alpha, beta, ply, pvIndex);

  // Generate moves for current board position
  MoveList myMoves = generateMoves(pos, true);

  if (myMoves.countMoves() == 0)
    return myMoves.checkers ? checkmateScore(ply) : VALUE_ZERO;

  if (pos.threeMoveRepetition() or pos.fiftyMoveDraw() or (!myMoves.exists<MType::CAPTURES>(pos) and isTheoreticalDraw(pos)))
    return VALUE_DRAW;

  info.addNode();

  if constexpr (USE_TT) {
    // TT_lookup (Check if given board is already in transpostion table)
    // check/stale-mate check needed before TT_lookup, else can lead to search failures.
    Score ttValue = tt.lookupPosition(pos.hashValue, depth, alpha, beta);

    if (ttValue != VALUE_UNKNOWN)
      return ttValue;
  }

  if constexpr (USE_EXTENSIONS) {
    int extensions = searchExtension(pos, myMoves, numExtensions, depth);
    depth += extensions;
    numExtensions += extensions;
  }

  Flag hashf = Flag::HASH_ALPHA;
  MoveArray movesArray;
  playAllMoves<0, MType::CAPTURES, MType::PROMOTION, MType::CHECK, MType::PV, MType::KILLER, MType::QUIET>
    (pos, myMoves, movesArray, 0, alpha, beta, depth, ply, pvIndex, numExtensions, hashf);

  if constexpr (USE_TT) {
    tt.recordPosition(pos.hashValue, depth, alpha, hashf);
  }

  return alpha;
}

Score
rootAlphaBeta(ChessBoard& pos, Score alpha, Score beta, Depth depth)
{
  perf_clock startTime;
  perf_ns_time duration;

  int ply{0}, pvIndex{0}, pvNextIndex;

  MoveArray myMoves = info.getMoves();

  pvArray[pvIndex] = NULL_MOVE; // no pv yet
  pvNextIndex = pvIndex + MAX_PLY - ply;

  for (size_t moveNo = 0; moveNo < myMoves.size(); ++moveNo)
  {
    Move move = myMoves[moveNo];
    startTime = perf::now();

    Score eval = playMove<rootReduction>(pos, move, moveNo, depth, alpha, beta, ply, pvNextIndex, 0);

    duration = perf::now() - startTime;
    info.insertMoveToList(moveNo);

    if (info.timeOver())
      return TIMEOUT;

    if (eval > alpha)
    {
      alpha = eval;
      pvArray[pvIndex] = filter(move);
      movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, MAX_PLY - ply - 1);
    }
  }

  return alpha;
}

void
search(ChessBoard board, Depth mDepth, double search_time, std::ostream& writer, bool debug)
{
  resetPvLine();

  if (generateMoves(board).countMoves() == 0)
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

    if (info.timeOver())
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

      info.addResult(board, eval, pvArray);
      if (debug)
        info.showLastDepthResult(board, writer);
      info.resetNodeCount();

      depth++;
    }

    // If found a checkmate
    if (withinValWindow and (__abs(eval) >= VALUE_INF - 500)) break;

    // Sort Moves according to time it took to explore the move.
    info.sortMovesOnTime(pvArray[0]);
  }

  info.searchCompleted();
  if (debug)
    writer << "Search Done!" << endl;
}
