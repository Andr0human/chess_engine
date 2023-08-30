

#include "multi_thread.h"

std::thread td[MAX_THREADS];
thread_search_info thread_data;
std::mutex mute;


#ifndef THREAD_SEARCH_INFO

thread_search_info::thread_search_info()
{
    threads_available = true;
    time_left = true;
    beta_cut = false;
}

void
thread_search_info::set(ChessBoard &tmp_board, MoveList &tmp,
    int t_dep, int ta, int tb, int tply, int pv_idx, int start)
{
    threads_available = beta_cut = false;
    NodeCount = 0;
    Board = tmp_board;
    for (size_t i = 0; i < tmp.size(); i++)
        legal_moves[i] = tmp.pMoves[i];
    
    moveCount = tmp.size(); index = start; depth = t_dep;
    ply = tply; pvIndex = pv_idx, best_move = tmp.pMoves[0];

    hashf = HASH_ALPHA;
    alpha = ta; beta = tb;
}

int
thread_search_info::get_index() noexcept
{
    if (index >= static_cast<int>(moveCount)) return -1;
    int value = index;
    index++;
    return value;
}

std::pair<int, int>
thread_search_info::result()
{ return std::make_pair(best_move, alpha); }

#endif

#ifndef NODECOUNT

uint64_t
bulk_MultiCount(ChessBoard &_cb, Depth depth)
{
    if (depth <= 0) return 1;

    MoveList myMoves = GenerateMoves(_cb);

    if (depth == 1) return myMoves.size();
    
    if (thread_data.threads_available)
    {
        thread_data.set(_cb, myMoves, depth, 0, 0, 0, 0, 0);
        run_threads('c', pvArray);
        return thread_data.NodeCount;
    }
    
    uint64_t answer = 0;
    for (const auto move : myMoves)
    {
        _cb.MakeMove(move);
        answer += bulk_MultiCount(_cb, depth - 1);
        _cb.UnmakeMove();
    }

    return answer;
}

#endif

#ifndef MULTI_THREAD_SEARCH

void
MakeMove_MultiIterative(ChessBoard &primary, int mDepth, double search_time)
{
    info = SearchData(primary, search_time);

    bool within_valWindow = true;
    Score alpha = -VALUE_INF, beta = VALUE_INF;
    int valWindowCnt = 0;
    
    for (Depth depth = 1; depth <= mDepth;)
    {
        // pre_status(depth, valWindowCnt);
        int eval = pv_multiAlphaBetaRoot(primary, alpha, beta, depth);
        // post_status(primary, pvArray[0], eval, global_time);

        if (__abs(eval) == VALUE_UNKNOWN) break;
        if ((eval <= alpha) || (eval >= beta))
        {
            // We fell outside the window, so try again with a wider Window
            valWindowCnt++;
            alpha = eval - (VALUE_WINDOW << valWindowCnt);
            beta = eval + (VALUE_WINDOW << valWindowCnt);
            within_valWindow = false;
        }
        else
        {
            // Set up the window for the next iteration.
            alpha = eval - VALUE_WINDOW;
            beta  = eval + VALUE_WINDOW;
            within_valWindow = true;
            valWindowCnt = 0;
            // info.update(depth, eval, pvArray);
            // curr_depth_status(primary);
            depth++;
        }
        if (within_valWindow and (__abs(eval) >= VALUE_INF - 500)) break;   // If found a checkmate
        moc.SortList(pvArray[0]);
    }
}

int
thread_AlphaBeta(ChessBoard &_cb, int loc_arr[], Score alpha, Score beta, Depth depth, int ply, int pvIndex)
{
    if (depth <= 0)
        return QuieSearch(_cb, alpha, beta, ply, 0);

    // info.max_ply = std::max(info.max_ply, ply);

    if (LegalMovesPresent(_cb) == false)
    {
        _cb.RemoveMovegenMetadata();
        return _cb.InCheck() ? CheckmateScore(ply) : 0;
    }

    int hashf = HASH_ALPHA, tt_val, eval;

    #if defined(TRANSPOSITION_TABLE_H)

        // if (ply && (tt_val = TT.ProbeSearchHistory(_cb.Hash_Value)) != VALUE_UNKNOWN) return 0;
        if ((tt_val = TT.LookupPosition(_cb.Hash_Value, depth, alpha, beta)) != VALUE_UNKNOWN) return tt_val;
        // TT.RecordSearch(_cb.Hash_Value);

    #endif

    auto myMoves = GenerateMoves(_cb);
    ReorderGeneratedMoves(myMoves, true);

    loc_arr[pvIndex] = 0; // no pv yet
    int pvNextIndex = pvIndex + MAX_PLY - ply;

    if (OkToDoLMR(depth, myMoves))
        return LMR_threadSearch(_cb, loc_arr, myMoves, depth, alpha, beta, ply, pvIndex);

    for (const auto move : myMoves)
    {
        _cb.MakeMove(move);
        eval = -thread_AlphaBeta(_cb, loc_arr, -beta, -alpha, depth - 1, ply + 1, pvNextIndex);
        _cb.UnmakeMove();
        
        if (__abs(eval) == VALUE_UNKNOWN) return VALUE_UNKNOWN;
        if (eval > alpha)
        {
            hashf = HASH_EXACT;
            alpha = eval;
            loc_arr[pvIndex] = move;
            movcpy (loc_arr + pvIndex + 1, loc_arr + pvNextIndex, MAX_PLY - ply - 1);
        }
        if (alpha >= beta)
        {
            #if defined(TRANSPOSITION_TABLE_H)
                TT.RecordPosition(_cb.Hash_Value, depth, move, beta, HASH_BETA);
                // TT.RemSearchHistory(_cb.Hash_Value);
            #endif

            return beta;
        }
    }

    #if defined(TRANSPOSITION_TABLE_H)
        // TT.RemSearchHistory(_cb.Hash_Value);
        TT.RecordPosition(_cb.Hash_Value, depth, loc_arr[pvIndex], alpha, hashf);
    #endif

    return alpha;
}

Score
pv_multiAlphaBeta(ChessBoard& _cb, int loc_arr[], Score alpha, Score beta, Depth depth, int ply, int pvIndex)
{
    if (depth <= 0)
        return QuieSearch(_cb, alpha, beta, ply, 0);

    // info.max_ply = std::max(info.max_ply, ply);
    if (LegalMovesPresent(_cb) == false)
    {
        _cb.RemoveMovegenMetadata();
        return _cb.InCheck() ? CheckmateScore(ply) : 0;
    }

    int hashf = HASH_ALPHA, tt_val;

    #if defined(TRANSPOSITION_TABLE_H)

        // if (ply && (tt_val = TT.ProbeSearchHistory(_cb.Hash_Value)) != VALUE_UNKNOWN) return 0;
        if ((tt_val = TT.LookupPosition(_cb.Hash_Value, depth, alpha, beta)) != VALUE_UNKNOWN) return tt_val;
        // TT.RecordSearch(_cb.Hash_Value);

    #endif

    auto myMoves = GenerateMoves(_cb);
    ReorderGeneratedMoves(myMoves, true);

    loc_arr[pvIndex] = 0; // no pv yet
    int pvNextIndex = pvIndex + MAX_PLY - ply;

    bool cut = fm_Search(_cb, loc_arr, depth, myMoves.pMoves[0], alpha, beta, ply, pvIndex);
    if (__abs(alpha) == VALUE_UNKNOWN) return VALUE_UNKNOWN;
    if (cut)
    {
        // TT.RecordHash (myMoves.pMoves[0], _cb.Hash_Value, depth, beta, HASHBETA);
        // TT.RemSearchHistory(_cb.Hash_Value);
        return beta;
    }

    if (thread_data.threads_available)
    {
        thread_data.set(_cb, myMoves, depth, alpha, beta, ply, pvIndex, 1);
        run_threads('a', loc_arr);
        
        if (__abs(thread_data.alpha) == VALUE_UNKNOWN) return VALUE_UNKNOWN;

        // TT.RemSearchHistory(_cb.Hash_Value);
        if (thread_data.beta_cut)
        {
            // TT.RecordHash (thread_data.best_move, _cb.Hash_Value, depth, beta, HASHBETA);
            return thread_data.beta;
        }

        // TT.RecordHash(thread_data.best_move, _cb.Hash_Value, depth, alpha, hashf);
        return thread_data.alpha;
    }

    for (const auto move : myMoves)
    {
        _cb.MakeMove(move);
        int eval = -thread_AlphaBeta(_cb, loc_arr, -beta, -alpha, depth - 1, ply + 1, pvNextIndex);
        _cb.UnmakeMove();

        if (__abs(eval) == VALUE_UNKNOWN) return VALUE_UNKNOWN;
        if (eval > alpha)
        {
            hashf = HASH_EXACT;
            alpha = eval;
            loc_arr[pvIndex] = move;
            movcpy (loc_arr + pvIndex + 1, loc_arr + pvNextIndex, MAX_PLY - ply - 1);
        }
        if (alpha >= beta)
        {
            // TT.RecordHash (move, _cb.Hash_Value, depth, beta, HASHBETA);
            // TT.RemSearchHistory(_cb.Hash_Value);
            return beta;
        }
    }
    
    // TT.RemSearchHistory(_cb.Hash_Value);

    #if defined(TRANSPOSITION_TABLE_H)
        TT.RecordPosition(_cb.Hash_Value, depth, loc_arr[pvIndex], alpha, hashf);
    #endif
    return alpha;
}

int
pv_multiAlphaBetaRoot(ChessBoard &_cb, Score alpha, Score beta, Depth depth)
{
    MoveList myMoves = GenerateMoves(_cb);
    moc.OrderMovesOnTime(myMoves);
    // moc.print(_cb);
    // moc.Reset();

    int ply = 0, pvIndex = 0, eval;
    pvArray[pvIndex] = 0;

    bool cut = first_rm_search(_cb, myMoves.pMoves[0], depth, alpha, beta);
    if (cut) return beta;

    thread_data.set(_cb, myMoves, depth, alpha, beta, ply, pvIndex, 1);
    run_threads('r', pvArray);

    eval = thread_data.beta_cut ? thread_data.beta : thread_data.alpha;
    return eval;

    /* 
    // METHOD 2:
    TIME_POINT startTime, endTime;
    TIME_VARIABLE duration;
    int pvNextIndex = pvIndex + MAX_PLY - ply;

    for (int i = 0; i < myMoves.__count; i++)
    {
        int move = myMoves.pMoves[i];
        startTime = TIME_NOW;
        if (i < LMR_LIMIT || InterestingMove(move, _cb))
        {
            _cb.MakeMove(move);
            eval = -pv_multiAlphaBeta(_cb, pvArray, -beta, -alpha, depth - 1, ply + 1, pvNextIndex);
            _cb.UnmakeMove();
        }
        else
        {
            int R = RootReduction(depth, i);
            _cb.MakeMove(move);
            eval = -pv_multiAlphaBeta(_cb, pvArray, -beta, -alpha, depth - 1 - R, ply + 1, pvNextIndex);
            _cb.UnmakeMove();
            if (eval > alpha && R)
            {
                startTime = TIME_NOW;
                _cb.MakeMove(move);
                eval = -pv_multiAlphaBeta(_cb, pvArray, -beta, -alpha, depth - 1, ply + 1, pvNextIndex);
                _cb.UnmakeMove();
            }
        }

        endTime = TIME_NOW;
        duration = TIME_BTW(startTime, endTime);
        moc.Insert(i, duration.count());

        if (__abs(eval) == VALUE_UNKNOWN) return VALUE_UNKNOWN;
        if (eval > alpha)
        {
            alpha = eval;
            pvArray[pvIndex] = move;
            movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, MAX_PLY - ply - 1);
        }
        if (alpha >= beta) return beta;
    }
    return alpha; */
}

int
LMR_threadSearch(ChessBoard &_cb, int loc_arr[], MoveList &myMoves, Depth depth, Score alpha, Score beta, int ply, int pvIndex)
{
    int eval, hashf = HASH_ALPHA;
    int pvNextIndex = pvIndex + MAX_PLY - ply;
    int __n = static_cast<int>(myMoves.size());

    for (int i = 0; i < __n; i++)
    {
        if (i < LMR_LIMIT || InterestingMove(myMoves.pMoves[i], _cb))
        {
            _cb.MakeMove(myMoves.pMoves[i]);
            eval = -thread_AlphaBeta(_cb, loc_arr, -beta, -alpha, depth - 1, ply + 1, pvNextIndex);
            _cb.UnmakeMove();
        }
        else
        {
            int R = Reduction(depth, i);
            _cb.MakeMove(myMoves.pMoves[i]);
            eval = -thread_AlphaBeta(_cb, loc_arr, -beta, -alpha, depth - 1 - R, ply + 1, pvNextIndex);
            _cb.UnmakeMove();
            if (__abs(eval) == VALUE_UNKNOWN) return VALUE_UNKNOWN;
            if (eval > alpha)
            {
                _cb.MakeMove(myMoves.pMoves[i]);
                eval = -thread_AlphaBeta(_cb, loc_arr, -beta, -alpha, depth - 1, ply + 1, pvNextIndex);
                _cb.UnmakeMove();
            }
        }
        if (__abs(eval) == VALUE_UNKNOWN) return VALUE_UNKNOWN;
        if (eval > alpha)
        {
            hashf = HASH_EXACT;
            alpha = eval;
            loc_arr[pvIndex] = myMoves.pMoves[i];
            movcpy (loc_arr + pvIndex + 1, loc_arr + pvNextIndex, MAX_PLY - ply - 1);
        }
        if (eval >= beta)
        {
            // TT.RecordHash (myMoves.pMoves[i], _cb.Hash_Value, depth, beta, HASHBETA);
            // TT.RemSearchHistory(_cb.Hash_Value);
            return beta;
        }
    }

    // TT.RemSearchHistory(_cb.Hash_Value);

    #if defined(TRANSPOSITION_TABLE_H)
        TT.RecordPosition(_cb.Hash_Value, depth, pvArray[pvIndex], alpha, hashf);
    #endif
    return alpha;
}

bool
fm_Search(ChessBoard &_cb, int loc_arr[], Depth depth, int move, int &alpha, int &beta, int ply, int pvIndex)
{
    // No need to pick Reduction as this is first move
    int pvNextIndex = pvIndex + MAX_PLY - ply;

    _cb.MakeMove(move);
    int eval = -pv_multiAlphaBeta(_cb, pvArray, -beta, -alpha, depth - 1, ply + 1, pvNextIndex);
    _cb.UnmakeMove();

    if (eval > alpha)
    {
        alpha = eval;
        loc_arr[pvIndex] = move;
        movcpy (loc_arr + pvIndex + 1, pvArray + pvNextIndex, MAX_PLY - ply - 1);
    }
    if (alpha >= beta) return true;
    return false;
}

bool
first_rm_search(ChessBoard &_cb, int move, Depth depth, int &alpha, int &beta)
{
    // rm_search => root_move_search

    perf_clock startTime;
    perf_time duration;

    int ply = 0, pvIndex = 0;
    int pvNextIndex = pvIndex + MAX_PLY - ply;

    startTime = perf::now();
    _cb.MakeMove(move);
    alpha = -pv_multiAlphaBeta(_cb, pvArray, -beta, -alpha, depth - 1, ply + 1, pvNextIndex);
    _cb.UnmakeMove();

    duration = perf::now() - startTime;
    moc.Insert(0, duration.count() * 10000);

    pvArray[pvIndex] = move;
    movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, MAX_PLY - ply - 1);

    if (alpha >= beta) return true;
    return false;
}

#endif

#ifndef WORKER

void
run_threads(char runtype, int sourceArr[])
{
    int threadCount = 4;
    if (runtype == 'c')
    {
        for (int i = 0; i < threadCount; i++)
            td[i] = std::thread(worker_Count, i);

        for (int i = 0; i < threadCount; i++)
            td[i].join();
    }
    else if (runtype == 'a')
    {
        for (int i = 0; i < threadCount; i++)
            td[i] = std::thread(worker_alphabeta, i, sourceArr);

        for (int i = 0; i < threadCount; i++)
            td[i].join();
    }
    else if (runtype == 'r')
    {
        for (int i = 0; i < threadCount; i++)
            td[i] = std::thread(worker_root_alphabeta, i, sourceArr);

        for (int i = 0; i < threadCount; i++)
            td[i].join();
    }

    thread_data.threads_available = true;
    return;
}

void
worker_Count(int thread_num)
{
    ChessBoard _cb;
    mute.lock();
    _cb = thread_data.Board;
    Depth depth = thread_data.depth;
    mute.unlock();

    int move = thread_num;
    uint64_t answer = 0;

    while (true)
    {
        mute.lock();
        int index = thread_data.get_index();
        mute.unlock();
        if (index == -1) break;
        move = thread_data.legal_moves[index];
        _cb.MakeMove(move);
        answer += bulk_MultiCount(_cb, depth - 1);
        _cb.UnmakeMove();
    }
    mute.lock();
    thread_data.NodeCount += answer;
    mute.unlock();
}

void
worker_alphabeta(int thread_num, int sourceArr[])
{
    ChessBoard _cb;
    mute.lock();
    _cb = thread_data.Board;
    Depth depth   = thread_data.depth;
    int ply     = thread_data.ply;
    int pvIndex = thread_data.pvIndex;
    mute.unlock();

    int pvNextIndex  = pvIndex + MAX_PLY - ply;
    int eval, move;

    while (true)
    {
        mute.lock();
        int index = thread_data.get_index();
        Score alpha = thread_data.alpha, beta = thread_data.beta;
        bool cut  = thread_data.beta_cut;
        mute.unlock();

        if (index == -1 || cut) break;
        
        move = thread_data.legal_moves[index];
        _cb.MakeMove(move);
        eval = -thread_AlphaBeta(_cb, thread_array[thread_num], -beta, -alpha, depth - 1, ply + 1, pvNextIndex);
        _cb.UnmakeMove();

        if (__abs(eval) == VALUE_UNKNOWN)
        {
            thread_data.alpha = VALUE_UNKNOWN;
            break;
        }

        mute.lock();

        if (eval > thread_data.alpha)
        {
            thread_data.alpha = eval;
            thread_data.best_move = move;
            sourceArr[pvIndex] = move;
            movcpy (sourceArr + pvIndex + 1, thread_array[thread_num] + pvNextIndex, MAX_PLY - ply - 1);
        }
        if (thread_data.alpha >= thread_data.beta) thread_data.beta_cut = true;

        mute.unlock();
    }
}

void
worker_root_alphabeta(int thread_num, int sourceArr[])
{
    ChessBoard _cb;
    mute.lock();
    _cb = thread_data.Board;
    Depth depth   = thread_data.depth;
    mute.unlock();

    int ply = 0, pvIndex = 0;
    int pvNextIndex  = pvIndex + MAX_PLY - ply;
    int eval, move;

    perf_clock startTime;
    perf_time duration;

    while (true)
    {
        mute.lock();
        int index = thread_data.get_index();
        Score alpha = thread_data.alpha, beta = thread_data.beta;
        bool cut  = thread_data.beta_cut;

        mute.unlock();

        if (index == -1 || cut) break;
        move = thread_data.legal_moves[index];

        startTime = perf::now();
        if (index < LMR_LIMIT || InterestingMove(move, _cb))
        {
            _cb.MakeMove(move);
            eval = -thread_AlphaBeta(_cb, thread_array[thread_num], -beta, -alpha, depth - 1, ply + 1, pvNextIndex);
            _cb.UnmakeMove();
        }
        else
        {
            int R = RootReduction(depth, index);
            _cb.MakeMove(move);
            eval = -thread_AlphaBeta(_cb, thread_array[thread_num], -beta, -alpha, depth - 1 - R, ply + 1, pvNextIndex);
            _cb.UnmakeMove();
            if (eval > alpha && R)
            {
                startTime = perf::now();
                _cb.MakeMove(move);
                eval = -thread_AlphaBeta(_cb, thread_array[thread_num], -beta, -alpha, depth - 1, ply + 1, pvNextIndex);
                _cb.UnmakeMove();
            }
        }

        if (__abs(eval) == VALUE_UNKNOWN)
        {
            thread_data.alpha = VALUE_UNKNOWN;
            break;
        }

        mute.lock();

        duration = perf::now() - startTime;
        moc.Insert(index, duration.count() * 10000);

        if (eval > thread_data.alpha)
        {
            thread_data.alpha = eval;
            thread_data.best_move = move;
            sourceArr[pvIndex] = move;
            movcpy (sourceArr + pvIndex + 1, thread_array[thread_num] + pvNextIndex, MAX_PLY - ply - 1);
        }
        if (thread_data.alpha >= thread_data.beta) thread_data.beta_cut = true;

        mute.unlock();
    }
}

#endif

