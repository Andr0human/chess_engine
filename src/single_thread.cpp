

#include "single_thread.h"

#ifndef SINGLE_THREAD_NEGAMAX

uint64_t
bulkcount(ChessBoard& _cb, Depth depth)
{
    if (depth <= 0) return 1;

    const MoveList myMoves = generate_moves(_cb);

    if (depth == 1) return myMoves.size();
    uint64_t answer = 0;

    for (const Move move : myMoves)
    {
        _cb.MakeMove(move);
        answer += bulkcount(_cb, depth - 1);
        _cb.UnmakeMove();
    }

    return answer;
}

Score
negaMax(ChessBoard& _cb, Depth depth)
{
    if (depth <= 0)
        return Evaluate(_cb);

    MoveList myMoves = generate_moves(_cb);
    if (myMoves.empty())
        return (_cb.king_in_check() ? checkmate_score(0) : 0);

    Score _eval = -VALUE_INF;

    for (const auto move : myMoves)
    {
        _cb.MakeMove(move);
        _eval = std::max(_eval, -negaMax(_cb, depth - 1));
        _cb.UnmakeMove();
    }
    return _eval;
}

std::pair<Move, Score>
negaMax_root(ChessBoard &_cb, Depth depth)
{
    if (has_legal_moves(_cb) == false)
    {
        Score score = _cb.king_in_check() ? checkmate_score(0) : VALUE_ZERO;
        _cb.remove_movegen_extra_data();
        return std::make_pair(NULL_MOVE, score);
    }
    
    const MoveList myMoves = generate_moves(_cb);

    Score best_eval = -VALUE_INF;
    Move  best_move =  NULL_MOVE;

    for (const auto move : myMoves)
    {
        _cb.MakeMove(move);
        Score eval = -negaMax(_cb, depth - 1);
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
alphabeta(ChessBoard& __pos, Depth depth, Score alpha, Score beta, int ply, int pvIndex, int numExtensions) 
{
    if (info.time_over())
        return TIMEOUT;

    {
        // check/stalemate check
        if (has_legal_moves(__pos) == false)
        {
            Score score = __pos.king_in_check() ? checkmate_score(ply) : VALUE_ZERO;
            __pos.remove_movegen_extra_data();
            return score;
        }

        // 3-move repetition check or 50-move-rule-draw
        if (__pos.three_move_repetition() or __pos.fifty_move_rule_draw())
        {
            __pos.remove_movegen_extra_data();
            return VALUE_DRAW;
        }
    }

    if (depth <= 0)
    {
        // Depth 0, starting Quiensense Search
        return QuieSearch(__pos, alpha, beta, ply, 0);
    }

    {
        // TT_lookup
        #if defined(TRANSPOSITION_TABLE_H)

            // Check for 3-move repetition

            // Check if given board is already in transpostion table
            // Note : Need to check for check-mate/stale-mate possibility before TT_lookup,
            //        else can lead to search failures.
            Score tt_val = TT.lookup_position(__pos.Hash_Value, depth, alpha, beta);
            if (tt_val != VALUE_UNKNOWN)
            {
                __pos.remove_movegen_extra_data();
                return tt_val;
            }
        #endif
    }

    // Generate moves for current board
    MoveList myMoves = generate_moves(__pos);

    // Order moves according to heuristics for faster alpha-beta search
    order_generated_moves(myMoves, true);


    // Search Extensions
    // int extensions = search_extension(__pos, numExtensions);
    // depth = depth + extensions;
    // numExtensions = numExtensions + extensions;

    // Try LMR_search,
    if (ok_to_do_LMR(depth, myMoves))
        return lmr_search(__pos, myMoves, depth, alpha, beta, ply, pvIndex, numExtensions);


    // Set pvArray, for storing the search_tree
    pvArray[pvIndex] = 0; // no pv yet
    int pvNextIndex = pvIndex + MAX_PLY - ply;
    int hashf = HASH_ALPHA;
    Score eval;

    for (const Move move : myMoves)
    {
        __pos.MakeMove(move);
        eval = -alphabeta(__pos, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
        __pos.UnmakeMove();

        // int moveNo = 1;
        // eval = play_move(__pos, move, moveNo, depth, alpha, beta, ply, pvNextIndex);


        // No time left!
        if (info.time_over())
            return TIMEOUT;

        if (eval >= beta)
        {
            // beta-cut found

            #if defined(TRANSPOSITION_TABLE_H)
                TT.record_position(__pos.Hash_Value, depth, move, beta, HASH_BETA);
            #endif
            
            return beta;
        }

        if (eval > alpha)
        {
            // Better move found, update the result
            hashf = HASH_EXACT;
            alpha = eval;
            pvArray[pvIndex] = move;
            movcpy (pvArray + pvIndex + 1,
                    pvArray + pvNextIndex, MAX_PLY - ply - 1);
        }
    }

    #if defined(TRANSPOSITION_TABLE_H)
        TT.record_position(__pos.Hash_Value, depth, pvArray[pvIndex], alpha, hashf);
    #endif

    return alpha;
}

Score
pv_root_alphabeta(ChessBoard& _cb, Score alpha, Score beta, Depth depth)
{
    perf_clock startTime;
    perf_ns_time duration;

    int ply{0}, pvIndex{0}, pvNextIndex, R, i = 0;
    Score eval;

    MoveList myMoves = generate_moves(_cb);
    moc.setMoveOrder(myMoves);


    pvArray[pvIndex] = 0; // no pv yet
    pvNextIndex = pvIndex + MAX_PLY - ply;

    for (const Move move : myMoves)
    {
        startTime = perf::now();
        if ((i < LMR_LIMIT) or interesting_move(move, _cb) or depth < 2)
        {
            _cb.MakeMove(move);
            eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, 0);
            _cb.UnmakeMove();
        }
        else
        {
            R = root_reduction(depth, i);
            _cb.MakeMove(move);
            eval = -alphabeta(_cb, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex, 0);
            _cb.UnmakeMove();

            if (info.time_over())
                return TIMEOUT;

            if ((R > 0) and (eval > alpha))
            {
                startTime = perf::now();
                _cb.MakeMove(move);
                eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, 0);
                _cb.UnmakeMove();
            }
        }

        duration = perf::now() - startTime;
        moc.insert(i, duration.count());

        if (info.time_over())
            return TIMEOUT;

        if (eval > alpha)
        {
            alpha = eval;
            pvArray[pvIndex] = myMoves.pMoves[i];
            movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, MAX_PLY - ply - 1);
        }
        // if (alpha >= beta) return beta;
        ++i;
    }

    return alpha;
}

Score
lmr_search(ChessBoard &_cb, MoveList& myMoves,
    Depth depth, Score alpha, Score beta, int ply, int pvIndex, int numExtensions)
{
    Score eval;
    int hashf = HASH_ALPHA, R, i = 0;
    int pvNextIndex = pvIndex + MAX_PLY - ply;

    for (const Move move : myMoves)
    {
        if ((i < LMR_LIMIT) or interesting_move(move, _cb))
        {
            _cb.MakeMove(move);
            eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
            _cb.UnmakeMove();
        }
        else
        {
            R = reduction(depth, i);
            
            _cb.MakeMove(move);
            eval = -alphabeta(_cb, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
            _cb.UnmakeMove();


            if (info.time_over())
                return TIMEOUT;

            if (eval > alpha)
            {
                _cb.MakeMove(move);
                eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
                _cb.UnmakeMove();
            }
        }

        if (info.time_over())
            return TIMEOUT;

        if (eval > alpha)
        {
            hashf = HASH_EXACT;
            alpha = eval;
            pvArray[pvIndex] = move;
            movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, MAX_PLY - ply - 1);
        }
        if (eval >= beta)
        {
            #if defined(TRANSPOSITION_TABLE_H)
                TT.record_position(_cb.Hash_Value, depth, move, beta, HASH_BETA);
            #endif

            return beta;
        }
        ++i;
    }

    #if defined(TRANSPOSITION_TABLE_H)
        TT.record_position(_cb.Hash_Value, depth, pvArray[pvIndex], alpha, hashf);
    #endif

    return alpha;
}



Score play_move(ChessBoard &_cb, Move move, int moveNo,
    Depth depth, Score alpha, Score beta, int ply, int pvNextIndex, int numExtensions)
{
    Score eval;

    if ((depth < 3) or (moveNo < LMR_LIMIT) or interesting_move(move, _cb))
    {
        // No reduction
        _cb.MakeMove(move);
        eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
        _cb.UnmakeMove();
    }
    else
    {
        int R = reduction(depth, moveNo);

        _cb.MakeMove(move);
        eval = -alphabeta(_cb, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
        _cb.UnmakeMove();

        //! TODO Test after removing this condition (looks unnecessary)
        if (info.time_over())
            return TIMEOUT;

        if (eval > alpha)
        {
            _cb.MakeMove(move);
            eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex, numExtensions);
            _cb.UnmakeMove();
        }
    }

    return eval;
}


void
search_iterative(ChessBoard board, Depth mDepth, double search_time, std::ostream& writer)
{
    reset_pv_line();

    if (has_legal_moves(board) == false)
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
        // writer << "Start root_Alphabeta for depth " << depth << endl;
        Score eval = pv_root_alphabeta(board, alpha, beta, depth);
        // writer << "End   root_Alphabeta for depth " << depth << endl;

        if (info.time_over())
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

            info.add_current_depth_result(depth, eval, pvArray);
            writer << info.show_last_depth_result(board) << endl;

            depth++;
        }

        // If found a checkmate
        if (within_valWindow and (__abs(eval) >= VALUE_INF - 500)) break;

        // Sort Moves according to time it took to explore the move.
        moc.sortList(pvArray[0]);
    }

    info.search_completed();
    writer << "Search Done!" << endl;
}

#endif
