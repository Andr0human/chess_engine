

#include "single_thread.h"

uint64_t
BulkCount(ChessBoard& _cb, Depth depth)
{
  if (depth <= 0) return 1;

  const MoveList myMoves = GenerateMoves(_cb);

  if (depth == 1) return myMoves.size();
  uint64_t answer = 0;

  for (const Move move : myMoves)
  {
    _cb.MakeMove(move);
    answer += BulkCount(_cb, depth - 1);
    _cb.UnmakeMove();
  }

  return answer;
}

Score
QuiescenceSearch(ChessBoard& pos, Score alpha, Score beta, Ply ply, int pvIndex)
{
  // Check if Time Left for Search
  if (info.TimeOver())
    return TIMEOUT;

  if (!LegalMovesPresent(pos))
    return pos.HandleScore(pos.InCheck() ? CheckmateScore(ply) : VALUE_ZERO);

  if (!CapturesExistInPosition(pos) and isTheoreticalDraw(pos))
    return pos.HandleScore(VALUE_DRAW);

  // Get a 'Stand Pat' Score
  Score stand_pat = Evaluate(pos);

  // Checking for beta-cutoff, usually called at the end of move-generation.
  if (stand_pat >= beta)
    return pos.HandleScore(beta);

  // int BIG_DELTA = 925;
  // if (stand_pat < alpha - BIG_DELTA) return alpha;

  if (stand_pat > alpha) alpha = stand_pat;

  MoveList myMoves = GenerateMoves(pos, true);

  if constexpr (useMoveOrder)
    OrderMoves(pos, myMoves, false, false);

  pvArray[pvIndex] = 0; // no pv yet
  int pvNextIndex = pvIndex + MAX_PLY - ply;

  for (const Move capture_move : myMoves)
  {
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
        pvArray[pvIndex] = filter(capture_move) | QS_MOVE;
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

  if (depth == 1 and FutilityOk(move))
  {
    Score margin = 2 * PawnValueMg;
    Score stand_pat = Evaluate(pos);

    if (stand_pat + margin < alpha)
      return alpha;
  }

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

Score
AlphaBeta(ChessBoard& pos, Depth depth, Score alpha, Score beta, Ply ply, int pvIndex, int numExtensions)
{
  if (info.TimeOver())
    return TIMEOUT;

  {
    // check/stalemate check
    if (!LegalMovesPresent(pos))
      return pos.HandleScore(pos.InCheck() ? CheckmateScore(ply) : VALUE_ZERO);

    // 3-move repetition check or 50-move-rule-draw
    if (pos.ThreeMoveRepetition() or pos.FiftyMoveDraw())
      return pos.HandleScore(VALUE_DRAW);
  }

  if (!CapturesExistInPosition(pos) and isTheoreticalDraw(pos))
    return pos.HandleScore(VALUE_DRAW);

  // Depth 0, starting Quiensense Search
  if (depth <= 0)
    return QuiescenceSearch(pos, alpha, beta, ply, pvIndex);

  if constexpr (useTT) {
    // TT_lookup (Check if given board is already in transpostion table)
    // check/stale-mate check needed before TT_lookup, else can lead to search failures.
    Score tt_val = TT.LookupPosition(pos.Hash_Value, depth, alpha, beta);

    if (tt_val != VALUE_UNKNOWN)
      return pos.HandleScore(tt_val);
  }

  // TODO: Try with findChecks on and off [or on-off with different depth]
  // Generate moves for current board
  MoveList myMoves = GenerateMoves(pos, false, true);

  // Order moves according to heuristics for faster alpha-beta search
  if constexpr (useMoveOrder)
    OrderMoves(pos, myMoves, true, true);

  if constexpr (useExtensions)
  {
    // Search Extensions
    int extensions = SearchExtension(pos, myMoves, numExtensions);
    depth         += extensions;
    numExtensions += extensions;
  }

  // Set pvArray, for storing the search_tree
  pvArray[pvIndex] = NULL_MOVE;
  int pvNextIndex = pvIndex + MAX_PLY - ply, hashf = HASH_ALPHA;
  Move bestMove = NULL_MOVE;

  for (size_t moveNo = 0; moveNo < myMoves.size(); ++moveNo)
  {
    Move move = myMoves.pMoves[moveNo];
    Score eval = PlayMove<Reduction>(pos, move, moveNo, depth, alpha, beta, ply, pvNextIndex, numExtensions);

    // No time left!
    if (info.TimeOver())
      return TIMEOUT;

    //! TODO: Why beta is not in root-search??
    // beta-cut found
    if (eval >= beta)
    {
      hashf = HASH_BETA, bestMove = move, alpha = beta;
      break;
    }

    // Better move found, update the result
    if (eval > alpha) {
      hashf = HASH_EXACT, bestMove = move, alpha = eval;
      pvArray[pvIndex] = filter(bestMove);
      movcpy (pvArray + pvIndex + 1,
              pvArray + pvNextIndex, MAX_PLY - ply - 1);
    }
  }

  if constexpr (useTT) {
    TT.RecordPosition(pos.Hash_Value, depth, bestMove, alpha, hashf);
  }

  return alpha;
}

Score
RootAlphabeta(ChessBoard& _cb, Score alpha, Score beta, Depth depth)
{
  perf_clock startTime;
  perf_ns_time duration;

  int ply{0}, pvIndex{0}, pvNextIndex;

  // MoveList myMoves = GenerateMoves(_cb, false, true);
  // moc.OrderMovesOnTime(myMoves);
  MoveList myMoves = info.getMoves();

  pvArray[pvIndex] = NULL_MOVE; // no pv yet
  pvNextIndex = pvIndex + MAX_PLY - ply;

  for (size_t moveNo = 0; moveNo < myMoves.size(); ++moveNo)
  {
    Move move = myMoves.pMoves[moveNo];
    startTime = perf::now();

    Score eval = PlayMove<RootReduction>(_cb, move, moveNo, depth, alpha, beta, ply, pvNextIndex, 0);

    duration = perf::now() - startTime;
    info.InsertMoveToList(moveNo, duration.count());

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

  if (LegalMovesPresent(board) == false)
  {
    writer << "Position has no legal moves! Discarding Search." << endl;
    return;
  }

  info = SearchData(board, search_time);

  bool within_valWindow = true;
  Score alpha = -VALUE_INF, beta = VALUE_INF;
  int valWindowCnt = 0;

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
      writer << info.ShowLastDepthResult(board) << endl;

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
