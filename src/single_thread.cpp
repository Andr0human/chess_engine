

#include "single_thread.h"

#ifndef SINGLE_THREAD_NEGAMAX

uint64_t
bulkcount(chessBoard &_cb, int depth)
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
negaMax(chessBoard &_cb, int depth)
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
negaMax_root(chessBoard &_cb, int depth)
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
search_iterative(chessBoard board, int mDepth, double search_time)
{
    // A Zero depth Move is produced in case we don't even have time to do a search of depth 1
    reset_pv_line();
    MoveType zero_move = createMoveOrderList(board);

    if (zero_move < 0)
    {
        cout << "No legal Moves on board! Discarding Search.\n";
        info.set_discard_result(zero_move);
        return;
    }

    info = SearchData(board.color, search_time, zero_move);

    bool within_valWindow = true;
    int alpha = negInf, beta = posInf, valWindowCnt = 0;

    for (int depth = 1; depth <= mDepth;)
    {
        // pre_status(depth, valWindowCnt);
        int eval = pv_root_alphabeta(board, alpha, beta, depth);
        // post_status(board, pvArray[0], eval, start_point);

        if (info.time_over())
            break;

        if ((eval <= alpha) || (eval >= beta))
        {
            // We fell outside the window, so try again with a wider Window
            valWindowCnt++;
            alpha = eval - (valWindow << valWindowCnt);
            beta  = eval + (valWindow << valWindowCnt);
            within_valWindow = false;
        }
        else
        {
            // Set up the window for the next iteration.
            alpha = eval - valWindow;
            beta  = eval + valWindow;
            within_valWindow = true;
            valWindowCnt = 0;

            info.add_current_depth_result(depth, eval, pvArray);
            // curr_depth_status(board);
            info.show_last_depth_result(board);
            depth++;
        }
        if (within_valWindow and (__abs(eval) >= (posInf >> 1) - 500)) break;   // If found a checkmate
        moc.sortList(pvArray[0]);                                               // Sort Moves according to time.
    }

    info.search_completed();
}

int
alphabeta(chessBoard& __pos, int depth,
    int alpha, int beta, int ply, int pvIndex) 
{
    if (info.time_over())
        return TIMEOUT;

    if (depth <= 0)
    {
        // Depth 0, starting Quiensense Search
        return QuieSearch(__pos, alpha, beta, ply, 0);
    }

    {
        // check/stalemate check
        if (has_legal_moves(__pos) == false)
        {
            int result = __pos.king_in_check() ? checkmate_score(ply) : 0;
            __pos.remove_movegen_extra_data();
            return result;
        }

        // 3-move repetition check
        if (__pos.three_move_repetition())
        {
            __pos.remove_movegen_extra_data();
            return DRAW_VALUE;
        }
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
        eval = -alphabeta(__pos, depth - 1,
                -beta, -alpha, ply + 1, pvNextIndex);

        __pos.UnmakeMove();


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
pv_root_alphabeta(chessBoard& _cb, int alpha, int beta, int depth)
{
    perf_clock startTime;
    perf_ns_time duration;

    int ply{0}, pvIndex{0}, eval, pvNextIndex, R, i = 0;
    
    MoveList myMoves = generate_moves(_cb);
    moc.setMoveOrder(myMoves);


    pvArray[pvIndex] = 0; // no pv yet
    pvNextIndex = pvIndex + maxPly - ply;

    for (const auto move : myMoves)
    {    
        startTime = perf::now();
        if ((i < LMR_LIMIT) or interesting_move(move, _cb))
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

            if ((eval > alpha) and R)
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
lmr_search(chessBoard &_cb, MoveList& myMoves,
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



#endif
