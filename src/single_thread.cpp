
#include "single_thread.h"

uint64_t
BulkCount(ChessBoard& pos, Depth depth)
{
  if (depth <= 0) return 1;

  const MoveList myMoves = GenerateMoves(pos, true);

  if (depth == 1)
    return myMoves.countMoves();

  MoveArray movesArray;
  myMoves.getMoves(pos, movesArray);

  uint64_t answer = 0;

  for (const Move move : movesArray)
  {
    pos.MakeMove(move);
    answer += BulkCount(pos, depth - 1);
    pos.UnmakeMove();
  }

  return answer;
}

template <bool leafnode = 0>
static Score
QuiescenceSearch(ChessBoard& pos, Score alpha, Score beta, Ply ply, int pvIndex)
{
  // Check if Time Left for Search
  if (info.TimeOver())
    return TIMEOUT;

  const MoveList myMoves = GenerateMoves(pos);

  if (myMoves.countMoves() == 0)
    return myMoves.checkers ? CheckmateScore(ply) : VALUE_ZERO;

  if (!myMoves.Exists<MType::CAPTURES>(pos) and isTheoreticalDraw(pos))
    return VALUE_DRAW;

  info.AddQNode();

  // Get a 'Stand Pat' Score
  Score stand_pat = Evaluate(pos);

  // Checking for beta-cutoff, usually called at the end of move-generation.
  if (stand_pat >= beta)
    return beta;

  // int BIG_DELTA = 925;
  // if (stand_pat < alpha - BIG_DELTA) return alpha;

  if (stand_pat > alpha) alpha = stand_pat;

  if (!myMoves.Exists<MType::CAPTURES>(pos))
    return alpha;

  MoveArray movesArray;
  myMoves.getMoves<MType::CAPTURES>(pos, movesArray);

  if constexpr (useMoveOrder)
    OrderMoves(pos, movesArray, MType::CAPTURES, 0);

  pvArray[pvIndex] = 0; // no pv yet
  int pvNextIndex = pvIndex + MAX_PLY - ply;

  const size_t LMP_THRESHOLD = movesArray.size() < 4 ? 1 : 3;

  for (size_t moveNo = 0; moveNo < movesArray.size(); ++moveNo)
  {
    Move capture_move = movesArray[moveNo];
    if (moveNo >= LMP_THRESHOLD and SeeScore(pos, capture_move) < 0)
      continue;

    pos.MakeMove(capture_move);
    Score score = -QuiescenceSearch(pos, -beta, -alpha, ply + 1, pvNextIndex);
    pos.UnmakeMove();

    if (info.TimeOver())
      return TIMEOUT;

    // Check for Beta-cutoff
    if (score >= beta) return beta;

    if (score > alpha)
    {
      alpha = score;

      if (ply < MAX_PLY)
      {
        pvArray[pvIndex] = filter(capture_move) | quiescenceMove();
        movcpy (pvArray + pvIndex + 1,
                pvArray + pvNextIndex, MAX_PLY - ply - 1);
      }
    }
  }

  return alpha;
}

template <ReductionFunc reductionFunction>
static Score
PlayMove(ChessBoard& pos, Move move, size_t moveNo,
  Depth depth, Score alpha, Score beta, Ply ply, int pvNextIndex, int numExtensions)
{
  Score eval = VALUE_ZERO;

  if (useLMR and LmrOk(move, depth, moveNo))
  {
    int R = reductionFunction(depth, moveNo);

    pos.MakeMove(move);
    eval = -AlphaBeta(pos, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
    pos.UnmakeMove();

    // if timed-out, eval will be highly negative thus following code won't execute
    if ((eval > alpha) and (R > 0))
    {
      pos.MakeMove(move);
      eval = -AlphaBeta(pos, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
      pos.UnmakeMove();
    }
  }
  else
  {
    // No Reduction
    pos.MakeMove(move);
    eval = -AlphaBeta(pos, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
    pos.UnmakeMove();
  }

  return eval;
}

static void
PlaySubsetMoves(
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
    Score eval = PlayMove<Reduction>(pos, move, moveNo, depth, alpha, beta, ply, pvNextIndex, numExtensions);

    // No time left!
    if (info.TimeOver())
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
PlayAllMoves(
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

  size_t end = orderType == MType::QUIET ? movesArray.size() : OrderMoves(pos, movesArray, orderType, ply, start);
  PlaySubsetMoves(pos, movesArray, start, end, alpha, beta, depth, ply, pvIndex, numExtensions, hashf);

  if (hashf == Flag::HASH_BETA)
    return;

  if constexpr (sizeof...(rest) > 0)
    PlayAllMoves<moveGen + 1, rest...>
      (pos, myMoves, movesArray, end, alpha, beta, depth, ply, pvIndex, numExtensions, hashf);
}

Score
AlphaBeta(ChessBoard& pos, Depth depth, Score alpha, Score beta, Ply ply, int pvIndex, int numExtensions)
{
  if (info.TimeOver())
    return TIMEOUT;

    // Depth 0, starting Quiensense Search
  if (depth <= 0)
    return QuiescenceSearch<1>(pos, alpha, beta, ply, pvIndex);

  // Generate moves for current board position
  MoveList myMoves = GenerateMoves(pos, true);

  if (myMoves.countMoves() == 0)
    return myMoves.checkers ? CheckmateScore(ply) : VALUE_ZERO;

  if (pos.ThreeMoveRepetition() or pos.FiftyMoveDraw() or (!myMoves.Exists<MType::CAPTURES>(pos) and isTheoreticalDraw(pos)))
    return VALUE_DRAW;

  info.AddNode();

  if constexpr (useTT) {
    // TT_lookup (Check if given board is already in transpostion table)
    // check/stale-mate check needed before TT_lookup, else can lead to search failures.
    Score tt_val = TT.LookupPosition(pos.Hash_Value, depth, alpha, beta);

    if (tt_val != VALUE_UNKNOWN)
      return tt_val;
  }

  if constexpr (useExtensions) {
    int extensions = SearchExtension(pos, myMoves, numExtensions, depth);
    depth += extensions;
    numExtensions += extensions;
  }

  Flag hashf = Flag::HASH_ALPHA;
  MoveArray movesArray;
  PlayAllMoves<0, MType::CAPTURES, MType::PROMOTION, MType::CHECK, MType::PV, MType::KILLER, MType::QUIET>
    (pos, myMoves, movesArray, 0, alpha, beta, depth, ply, pvIndex, numExtensions, hashf);

  if constexpr (useTT) {
    TT.RecordPosition(pos.Hash_Value, depth, alpha, hashf);
  }

  return alpha;
}

Score
RootAlphabeta(ChessBoard& _cb, Score alpha, Score beta, Depth depth)
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

    Score eval = PlayMove<RootReduction>(_cb, move, moveNo, depth, alpha, beta, ply, pvNextIndex, 0);

    duration = perf::now() - startTime;
    info.InsertMoveToList(moveNo);

    if (info.TimeOver())
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
Search(ChessBoard board, Depth mDepth, double search_time, std::ostream& writer)
{
  ResetPvLine();

  if (GenerateMoves(board).countMoves() == 0)
  {
    writer << "Position has no legal moves! Discarding Search." << endl;
    return;
  }

  info = SearchData(board, search_time);

  bool within_valWindow = true;
  Score alpha = -VALUE_INF, beta = VALUE_INF;
  int valWindowCnt = 0;

  info.ShowHeader(writer);

  for (Depth depth = 1; depth <= mDepth;)
  {
    Score eval = RootAlphabeta(board, alpha, beta, depth);

    if (info.TimeOver())
      break;

    if ((eval <= alpha) or (eval >= beta))
    {
      // We fell outside the window, so try again with a wider Window
      valWindowCnt++;
      alpha = eval - (VALUE_WINDOW << valWindowCnt);
      beta  = eval + (VALUE_WINDOW << valWindowCnt);
      within_valWindow = false;
    }
    else
    {
      // Set up the window for the next iteration.
      alpha = eval - VALUE_WINDOW;
      beta  = eval + VALUE_WINDOW;
      within_valWindow = true;
      valWindowCnt = 0;

      info.AddResult(board, eval, pvArray);
      info.ShowLastDepthResult(board, writer);
      info.ResetNodeCount();

      depth++;
    }

    // If found a checkmate
    if (within_valWindow and (__abs(eval) >= VALUE_INF - 500)) break;

    // Sort Moves according to time it took to explore the move.
    info.SortMovesOnTime(pvArray[0]);
  }

  info.SearchCompleted();
  writer << "Search Done!" << endl;
}
