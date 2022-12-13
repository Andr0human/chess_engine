


#ifndef MULTI_THREAD_H
#define MULTI_THREAD_H

#include "bitboard.h"
#include "movegen.h"
#include "search.h"
// #include "new_eval.h"


struct thread_search_info
{
    chessBoard Board;
    bool beta_cut, threads_available, time_left;

    uint64_t moveCount, index;
    int depth;
    int ply, pvIndex, best_move, hashf;
    int legal_moves[156], alpha, beta;
    uint64_t NodeCount;

    thread_search_info ();

    void
    set(chessBoard &tmp_board, MoveList &tmp, int t_dep, int ta, int tb, int tply, int pv_idx, int start);

    uint64_t
    get_index();

    std::pair<int, int>
    result();
};



void run_threads(char runtype, int sourceArr[]);
void worker_Count(int thread_num);
void worker_alphabeta(int thread_num, int sourceArr[]);
void worker_root_alphabeta(int thread_num, int sourceArr[]);

uint64_t bulk_MultiCount(chessBoard &_cb, int depth);
void MakeMove_MultiIterative(chessBoard &primary, int mDepth = maxDepth, bool use_timer = true);
int thread_AlphaBeta(chessBoard &_cb, int loc_arr[], int alpha, int beta, int depth, int ply, int pvIndex);
int pv_multiAlphaBeta(chessBoard &_cb, int loc_arr[], int alpha, int beta, int depth, int ply, int pvIndex);
int pv_multiAlphaBetaRoot(chessBoard &_cb, int alpha, int beta, int depth);
int LMR_threadSearch(chessBoard &_cb, int loc_arr[], MoveList &myMoves, int depth, int alpha, int beta, int ply, int pvIndex);
bool fm_Search(chessBoard &_cb, int loc_arr[], int depth, int move, int &alpha, int &beta, int ply, int pvIndex);
bool first_rm_search(chessBoard &_cb, int __move, int depth, int &alpha, int &beta);

extern std::thread td[maxThreadCount];
extern thread_search_info thread_data;


#endif



