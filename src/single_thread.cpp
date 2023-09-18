

#include "single_thread.h"

#ifndef SINGLE_THREAD_NEGAMAX

typedef int (*ReductionFunc)(Depth depth, size_t move_no);

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
NegaMax(ChessBoard& _cb, Depth depth)
{
    if (depth <= 0)
        return Evaluate(_cb);

    MoveList myMoves = GenerateMoves(_cb);
    if (myMoves.empty())
        return (_cb.InCheck() ? CheckmateScore(0) : 0);

    Score _eval = -VALUE_INF;

    for (const auto move : myMoves)
    {
        _cb.MakeMove(move);
        _eval = std::max(_eval, -NegaMax(_cb, depth - 1));
        _cb.UnmakeMove();
    }
    return _eval;
}

std::pair<Move, Score>
RootNegaMax(ChessBoard &_cb, Depth depth)
{
    if (LegalMovesPresent(_cb) == false)
    {
        Score score = _cb.InCheck() ? CheckmateScore(0) : VALUE_ZERO;
        _cb.RemoveMovegenMetadata();
        return std::make_pair(NULL_MOVE, score);
    }

    const MoveList myMoves = GenerateMoves(_cb);

    Score best_eval = -VALUE_INF;
    Move  best_move =  NULL_MOVE;

    for (const auto move : myMoves)
    {
        _cb.MakeMove(move);
        Score eval = -NegaMax(_cb, depth - 1);
        _cb.UnmakeMove();

        if (eval > best_eval)
        {
            best_move = move;
            best_eval = eval;
        }
    }
    return std::make_pair(best_move, best_eval);
}

#endif


#ifndef SINGLE_THREAD_SEARCH

Score
QuiescenceSearch(ChessBoard& pos, Score alpha, Score beta, Ply ply, int pvIndex)
{
    // Check if Time Left for Search
    if (info.TimeOver())
        return TIMEOUT;

    if (!LegalMovesPresent(pos))
    {
        Score score = pos.InCheck() ? CheckmateScore(ply) : VALUE_ZERO;
        pos.RemoveMovegenMetadata();
        return score;
    }

    // if (ka_pieces.attackers) return AlphaBetaNonPV(_cb, 1, alpha, beta, ply);

    // Get a 'Stand Pat' Score
    Score stand_pat = Evaluate(pos);

    // Checking for beta-cutoff
    if (stand_pat >= beta)
    {
        // Usually called at the end of move-generation.
        pos.RemoveMovegenMetadata();
        return beta;
    }

    // int BIG_DELTA = 925;
    // if (stand_pat < alpha - BIG_DELTA) return alpha;

    if (stand_pat > alpha) alpha = stand_pat;

    MoveList myMoves = GenerateMoves(pos, true);
    OrderMoves(myMoves, false, false);


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
                pvArray[pvIndex] = filter(capture_move) | QUIESCENCE_FLAG;
                movcpy (pvArray + pvIndex + 1,
                        pvArray + pvNextIndex, MAX_PLY - ply - 1);
            }
        }
    }

    return alpha;
}

static bool
LmrOk(ChessBoard& pos, Depth depth, Move move, size_t moveNo)
{
    if ((depth < 3) or (moveNo < LMR_LIMIT) or InterestingMove(move, pos))
        return false;

    return true;
}


template <ReductionFunc reductionFunction>
static Score
PlayMove(ChessBoard& pos, Move move, size_t moveNo,
    Depth depth, Score alpha, Score beta, Ply ply, int pvNextIndex, int numExtensions)
{
    Score eval = VALUE_ZERO;

    if (LmrOk(pos, depth, move, moveNo))
    {
        int R = reductionFunction(depth, moveNo);

        pos.MakeMove(move);
        eval = -AlphaBeta(pos, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
        pos.UnmakeMove();

        //! TODO Test after removing this condition (looks unnecessary)
        if (info.TimeOver())
            return TIMEOUT;

        if (eval > alpha)
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
AlphaBeta(ChessBoard& __pos, Depth depth, Score alpha, Score beta, Ply ply, int pvIndex, int numExtensions) 
{
    if (info.TimeOver())
        return TIMEOUT;

    {
        // check/stalemate check
        if (LegalMovesPresent(__pos) == false)
        {
            Score score = __pos.InCheck() ? CheckmateScore(ply) : VALUE_ZERO;
            __pos.RemoveMovegenMetadata();
            return score;
        }

        // 3-move repetition check or 50-move-rule-draw
        if (__pos.ThreeMoveRepetition() or __pos.FiftyMoveDraw())
        {
            __pos.RemoveMovegenMetadata();
            return VALUE_DRAW;
        }
    }

    if (depth <= 0)
    {
        // Depth 0, starting Quiensense Search
        return QuiescenceSearch(__pos, alpha, beta, ply, pvIndex);
    }

    {
        // TT_lookup
        #if defined(TRANSPOSITION_TABLE_H)

            // Check for 3-move repetition

            // Check if given board is already in transpostion table
            // Note : Need to check for check-mate/stale-mate possibility before TT_lookup,
            //        else can lead to search failures.
            Score tt_val = TT.LookupPosition(__pos.Hash_Value, depth, alpha, beta);
            if (tt_val != VALUE_UNKNOWN)
            {
                __pos.RemoveMovegenMetadata();
                return tt_val;
            }
        #endif
    }

    // Generate moves for current board
    MoveList myMoves  = GenerateMoves(__pos, false, true);

    // Order moves according to heuristics for faster alpha-beta search
    OrderMoves(myMoves, true, true);


    // Search Extensions
    // int extensions = SearchExtension(__pos, numExtensions);
    // depth = depth + extensions;
    // numExtensions = numExtensions + extensions;

    // Try LMR_search,
    // if (OkToDoLMR(depth, myMoves))
    //     return LmrSearch(__pos, myMoves, depth, alpha, beta, ply, pvIndex, numExtensions);


    // Set pvArray, for storing the search_tree
    pvArray[pvIndex] = NULL_MOVE;
    int pvNextIndex = pvIndex + MAX_PLY - ply;

    int hashf = HASH_ALPHA;

    for (size_t moveNo = 0; moveNo < myMoves.size(); ++moveNo)
    {
        Move move = myMoves.pMoves[moveNo];
        Score eval = PlayMove<Reduction>(__pos, move, moveNo, depth, alpha, beta, ply, pvNextIndex, numExtensions);
        // __pos.MakeMove(move);
        // eval = -AlphaBeta(__pos, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
        // __pos.UnmakeMove();

        // No time left!
        if (info.TimeOver())
            return TIMEOUT;

        if (eval >= beta)
        {
            // beta-cut found

            #if defined(TRANSPOSITION_TABLE_H)
                TT.RecordPosition(__pos.Hash_Value, depth, move, beta, HASH_BETA);
            #endif
            
            return beta;
        }

        if (eval > alpha)
        {
            // Better move found, update the result
            hashf = HASH_EXACT;
            alpha = eval;
            pvArray[pvIndex] = filter(move);
            movcpy (pvArray + pvIndex + 1,
                    pvArray + pvNextIndex, MAX_PLY - ply - 1);
        }
    }

    #if defined(TRANSPOSITION_TABLE_H)
        TT.RecordPosition(__pos.Hash_Value, depth, pvArray[pvIndex], alpha, hashf);
    #endif

    return alpha;
}

Score
RootAlphabeta(ChessBoard& _cb, Score alpha, Score beta, Depth depth)
{
    perf_clock startTime;
    perf_ns_time duration;

    int ply{0}, pvIndex{0}, pvNextIndex/* , R, i = 0 */;
    Score eval;

    MoveList myMoves = GenerateMoves(_cb, false, true);
    moc.OrderMovesOnTime(myMoves);


    pvArray[pvIndex] = NULL_MOVE; // no pv yet
    pvNextIndex = pvIndex + MAX_PLY - ply;

    for (size_t moveNo = 0; moveNo < myMoves.size(); ++moveNo)
    {
        Move move = myMoves.pMoves[moveNo];
        startTime = perf::now();
        /* if ((i < LMR_LIMIT) or InterestingMove(move, _cb) or depth < 2)
        {
            _cb.MakeMove(move);
            eval = -AlphaBeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, 0);
            _cb.UnmakeMove();
        }
        else
        {
            R = RootReduction(depth, i);
            _cb.MakeMove(move);
            eval = -AlphaBeta(_cb, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex, 0);
            _cb.UnmakeMove();

            if (info.TimeOver())
                return TIMEOUT;

            if ((R > 0) and (eval > alpha))
            {
                startTime = perf::now();
                _cb.MakeMove(move);
                eval = -AlphaBeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, 0);
                _cb.UnmakeMove();
            }
        } */
        
        eval = PlayMove<RootReduction>(_cb, move, moveNo, depth, alpha, beta, ply, pvNextIndex, 0);
        duration = perf::now() - startTime;
        moc.Insert(moveNo, duration.count());

        if (info.TimeOver())
            return TIMEOUT;

        if (eval > alpha)
        {
            alpha = eval;
            pvArray[pvIndex] = filter(move);
            movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, MAX_PLY - ply - 1);
        }
        // if (alpha >= beta) return beta;
        // ++i;
    }

    return alpha;
}

/* Score
LmrSearch(ChessBoard &_cb, MoveList& myMoves,
    Depth depth, Score alpha, Score beta, Ply ply, int pvIndex, int numExtensions)
{
    Score eval;
    int hashf = HASH_ALPHA, R, i = 0;

    pvArray[pvIndex] = NULL_MOVE; // no pv yet
    int pvNextIndex = pvIndex + MAX_PLY - ply;

    for (const Move move : myMoves)
    {
        if ((i < LMR_LIMIT) or InterestingMove(move, _cb))
        {
            _cb.MakeMove(move);
            eval = -AlphaBeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
            _cb.UnmakeMove();
        }
        else
        {
            R = Reduction(depth, i);
            
            _cb.MakeMove(move);
            eval = -AlphaBeta(_cb, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
            _cb.UnmakeMove();


            if (info.TimeOver())
                return TIMEOUT;

            if (eval > alpha)
            {
                _cb.MakeMove(move);
                eval = -AlphaBeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
                _cb.UnmakeMove();
            }
        }

        if (info.TimeOver())
            return TIMEOUT;

        if (eval > alpha)
        {
            hashf = HASH_EXACT;
            alpha = eval;
            pvArray[pvIndex] = filter(move);
            movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, MAX_PLY - ply - 1);
        }
        if (eval >= beta)
        {
            #if defined(TRANSPOSITION_TABLE_H)
                TT.RecordPosition(_cb.Hash_Value, depth, move, beta, HASH_BETA);
            #endif

            return beta;
        }
        ++i;
    }

    #if defined(TRANSPOSITION_TABLE_H)
        TT.RecordPosition(_cb.Hash_Value, depth, pvArray[pvIndex], alpha, hashf);
    #endif

    return alpha;
}
 */


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
        moc.SortList(pvArray[0]);
    }

    info.SearchCompleted();
    writer << "Search Done!" << endl;
}

#endif
