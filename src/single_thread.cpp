

#include "single_thread.h"

#ifndef SINGLE_THREAD_NEGAMAX

uint64_t
bulkcount(ChessBoard &_cb, int depth)
{
    if (depth <= 0) return 1;

    // if (has_legal_moves(_cb) == false) return 0;
    const auto myMoves = generate_moves(_cb);

    if (depth == 1) return myMoves.size();
    uint64_t answer = 0;

    for (const auto move : myMoves)
    {
        _cb.MakeMove(move);
        answer += bulkcount(_cb, depth - 1);
        _cb.UnmakeMove();
    }

    return answer;
}

int
negaMax(ChessBoard &_cb, int depth)
{
    if (depth <= 0)
        return ev.Evaluate(_cb);

    MoveList myMoves = generate_moves(_cb);
    if (myMoves.empty())
        return (_cb.king_in_check() ? checkmate_score(0) : 0);

    int _eval = negInf;

    for (const auto move : myMoves)
    {
        _cb.MakeMove(move);
        _eval = std::max(_eval, -negaMax(_cb, depth - 1));
        _cb.UnmakeMove();
    }
    return _eval;
}

std::pair<MoveType, int>
negaMax_root(ChessBoard &_cb, int depth)
{
    if (has_legal_moves(_cb) == false)
    {
        int result = _cb.king_in_check() ? checkmate_score(0) : 0;
        _cb.remove_movegen_extra_data();
        return std::make_pair(0, result);
    }
    
    const auto myMoves = generate_moves(_cb);

    int best = negInf, _bm = 0, eval;

    for (const auto move : myMoves)
    {
        _cb.MakeMove(move);
        eval = -negaMax(_cb, depth - 1);
        _cb.UnmakeMove();
        if (eval > best)
        {
            _bm = move;
            best = eval;
        }
    }
    return std::make_pair(_bm, best);
}

#endif


#ifndef SINGLE_THREAD_SEARCH


void
search_iterative(ChessBoard board, int mDepth, double search_time, std::ostream& writer)
{
    reset_pv_line();

    if (has_legal_moves(board) == false)
    {
        puts("Position has no legal moves! Discarding Search.");
        return;
    }

    info = SearchData(board, search_time);

    bool within_valWindow = true;
    int alpha = negInf, beta = posInf, valWindowCnt = 0;

    for (int depth = 1; depth <= mDepth;)
    {
        writer << "Start Alphabeta for depth " << depth << endl;
        int eval = pv_root_alphabeta(board, alpha, beta, depth);
        writer << "End   Alphabeta for depth " << depth << endl;

        if (info.time_over()) {
            writer << "Timeout called!" << endl;
            break;
        }

        if ((eval <= alpha) or (eval >= beta))
        {
            writer << "Fell outside the window" << endl;
            // We fell outside the window, so try again with a wider Window
            valWindowCnt++;
            alpha = eval - (valWindow << valWindowCnt);
            beta  = eval + (valWindow << valWindowCnt);
            within_valWindow = false;
        }
        else
        {
            writer << "Can set next iteration." << endl;
            // Set up the window for the next iteration.
            alpha = eval - valWindow;
            beta  = eval + valWindow;
            within_valWindow = true;
            valWindowCnt = 0;

            info.add_current_depth_result(depth, eval, pvArray);
            writer << "Added result for current iteration!" << endl;
            writer << info.show_last_depth_result(board) << endl;

            depth++;
            writer << "Values for next iteration set!" << endl;
        }

        writer << "End Iteration" << endl;
        // If found a checkmate
        if (within_valWindow and (__abs(eval) >= (posInf >> 1) - 500)) break;

        // Sort Moves according to time it took to explore the move.
        moc.sortList(pvArray[0]);
        writer << "Moves Sorted for next Iteration!\n" << endl;
    }

    info.search_completed();
    writer << "Search Done!" << endl;
}

int
alphabeta(ChessBoard& __pos, int depth,
    int alpha, int beta, int ply, int pvIndex) 
{
    if (info.time_over())
        return TIMEOUT;


    {
        // check/stalemate check
        if (has_legal_moves(__pos) == false)
        {
            int result = __pos.king_in_check() ? checkmate_score(ply) : 0;
            __pos.remove_movegen_extra_data();
            return result;
        }

        // 3-move repetition check or 50-move-rule-draw
        if (__pos.three_move_repetition() or __pos.fifty_move_rule_draw())
        {
            __pos.remove_movegen_extra_data();
            return DRAW_VALUE;
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
            int tt_val = TT.lookup_position(__pos.Hash_Value, depth, alpha, beta);
            if (tt_val != valUNKNOWN)
            {
                __pos.remove_movegen_extra_data();
                return tt_val;
            }
        #endif
    }

    // Generate moves for current board
    auto myMoves = generate_moves(__pos);

    // Order moves according to heuristics for faster alpha-beta search
    order_generated_moves(myMoves, true);

    // Try LMR_search,
    if (ok_to_do_LMR(depth, myMoves))
        return lmr_search(__pos, myMoves, depth, alpha, beta, ply, pvIndex);


    // Set pvArray, for storing the search_tree
    pvArray[pvIndex] = 0; // no pv yet
    int pvNextIndex = pvIndex + maxPly - ply;
    int hashf = HASHALPHA, eval;

    for (const auto move : myMoves)
    {
        __pos.MakeMove(move);
        eval = -alphabeta(__pos, depth - 1, -beta, -alpha, ply + 1, pvNextIndex);
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
                TT.record_position(__pos.Hash_Value, depth, move, beta, HASHBETA);
            #endif
            
            return beta;
        }

        if (eval > alpha)
        {
            // Better move found, update the result
            hashf = HASHEXACT;
            alpha = eval;
            pvArray[pvIndex] = move;
            movcpy (pvArray + pvIndex + 1,
                    pvArray + pvNextIndex, maxPly - ply - 1);
        }
    }

    #if defined(TRANSPOSITION_TABLE_H)
        TT.record_position(__pos.Hash_Value, depth, pvArray[pvIndex], alpha, hashf);
    #endif

    return alpha;
}


int
pv_root_alphabeta(ChessBoard& _cb, int alpha, int beta, int depth)
{
    perf_clock startTime;
    perf_ns_time duration;

    int ply{0}, pvIndex{0}, eval, pvNextIndex, R, i = 0;
    
    MoveList myMoves = generate_moves(_cb);
    moc.setMoveOrder(myMoves);


    pvArray[pvIndex] = 0; // no pv yet
    pvNextIndex = pvIndex + maxPly - ply;

    for (const MoveType move : myMoves)
    {
        startTime = perf::now();
        if ((i < LMR_LIMIT) or interesting_move(move, _cb) or depth < 2)
        {
            _cb.MakeMove(move);
            eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex);
            _cb.UnmakeMove();
        }
        else
        {
            R = root_reduction(depth, i);
            _cb.MakeMove(move);
            eval = -alphabeta(_cb, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex);
            _cb.UnmakeMove();

            if (info.time_over())
                return TIMEOUT;

            if ((R > 0) and (eval > alpha))
            {
                startTime = perf::now();
                _cb.MakeMove(move);
                eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex);
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
            movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, maxPly - ply - 1);
        }
        // if (alpha >= beta) return beta;
        ++i;
    }

    return alpha;
}

int
lmr_search(ChessBoard &_cb, MoveList& myMoves,
    int depth, int alpha, int beta, int ply, int pvIndex)
{
    int eval, hashf = HASHALPHA, R, i = 0;
    int pvNextIndex = pvIndex + maxPly - ply;

    for (const auto move : myMoves)
    {
        if (i < LMR_LIMIT || interesting_move(move, _cb))
        {
            _cb.MakeMove(move);
            eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex);
            _cb.UnmakeMove();
        }
        else
        {
            R = reduction(depth, i);
            
            _cb.MakeMove(move);
            eval = -alphabeta(_cb, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex);
            _cb.UnmakeMove();


            if (info.time_over())
                return TIMEOUT;

            if (eval > alpha)
            {
                _cb.MakeMove(move);
                eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex);
                _cb.UnmakeMove();
            }
        }

        if (info.time_over())
            return TIMEOUT;

        if (eval > alpha)
        {
            hashf = HASHEXACT;
            alpha = eval;
            pvArray[pvIndex] = move;
            movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, maxPly - ply - 1);
        }
        if (eval >= beta)
        {
            #if defined(TRANSPOSITION_TABLE_H)
                TT.record_position(_cb.Hash_Value, depth, move, beta, HASHBETA);
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



int play_move(ChessBoard &_cb, int move, int moveNo,
    int depth, int alpha, int beta, int ply, int pvNextIndex)
{
    int eval;

    if ((depth < 3) or (moveNo < LMR_LIMIT) or interesting_move(move, _cb))
    {
        // No reduction
        _cb.MakeMove(move);
        eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex);
        _cb.UnmakeMove();
    }
    else
    {
        int R = reduction(depth, moveNo);

        _cb.MakeMove(move);
        eval = -alphabeta(_cb, depth - 1 - R, -beta, -alpha, ply + 1, pvNextIndex);
        _cb.UnmakeMove();

        //! TODO Test after removing this condition (looks unnecessary)
        if (info.time_over())
            return TIMEOUT;

        if (eval > alpha)
        {
            _cb.MakeMove(move);
            eval = -alphabeta(_cb, depth - 1, -beta, -alpha, ply + 1, pvNextIndex);
            _cb.UnmakeMove();
        }
    }

    return eval;
}


#endif
