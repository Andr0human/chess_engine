


#ifndef MULTI_THREAD_H
#define MULTI_THREAD_H

#include "bitboard.h"
#include "movegen.h"
#include "search.h"
#include <thread>
#include <mutex>


struct thread_search_info
{
    ChessBoard Board;
    bool beta_cut, threads_available, time_left;

    uint64_t moveCount;
    int index;
    int depth;
    int ply, pvIndex, best_move, hashf;
    int legal_moves[156], alpha, beta;
    uint64_t NodeCount;

    thread_search_info ();

    void
    set(ChessBoard &tmp_board, MoveList &tmp, int t_dep, int ta, int tb, int tply, int pv_idx, int start);

    int
    get_index() noexcept;

    std::pair<int, int>
    result();
};



void
run_threads(char runtype, int sourceArr[]);

void
worker_Count(int thread_num);

void
worker_alphabeta(int thread_num, int sourceArr[]);

void
worker_root_alphabeta(int thread_num, int sourceArr[]);




uint64_t
bulk_MultiCount(ChessBoard &_cb, int depth);

void
MakeMove_MultiIterative(ChessBoard &primary, int mDepth = MAX_DEPTH, double search_time = DEFAULT_SEARCH_TIME);

int
thread_AlphaBeta(ChessBoard &_cb, int loc_arr[], int alpha, int beta, int depth, int ply, int pvIndex);

int
pv_multiAlphaBeta(ChessBoard &_cb, int loc_arr[], int alpha, int beta, int depth, int ply, int pvIndex);

int
pv_multiAlphaBetaRoot(ChessBoard &_cb, int alpha, int beta, int depth);

int
LMR_threadSearch(ChessBoard &_cb, int loc_arr[], MoveList &myMoves, int depth, int alpha, int beta, int ply, int pvIndex);

bool
fm_Search(ChessBoard &_cb, int loc_arr[], int depth, int move, int &alpha, int &beta, int ply, int pvIndex);

bool
first_rm_search(ChessBoard &_cb, int __move, int depth, int &alpha, int &beta);

extern std::thread td[MAX_THREADS];
extern thread_search_info thread_data;
extern std::mutex mute;


#endif



