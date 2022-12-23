
#ifndef SEARCH_H
#define SEARCH_H

#include "perf.h"
#include "bitboard.h"
#include "movegen.h"
#include "search_utils.h"
#include "evaluation.h"
// #include "new_eval.h"
#include <thread>
#include <mutex>


// Variables
extern std::mutex mute;
extern bool search_time_left, extra_time_left, perf_test;
extern double alloted_search_time, alloted_extra_time;
extern uint64_t nodes_hits, qnodes_hits;
extern int threadCount;
extern const double default_allocate_time;
extern int pvArray[(maxPly * maxPly + maxPly) / 2];
extern int thread_array[maxThreadCount][(maxPly * maxPly) / 2];
extern const string default_fen;

// Utility
void timer();
void movcpy (int* pTarget, const int* pSource, int n);
void PRINT_LINE(chessBoard _cb, int line[], int __N);
void PRINT_LINE(chessBoard _cb, std::vector<int> line);
void pre_status(int __dep, int __cnt);
void post_status(chessBoard &_cb, int _m, int _e, perf_clock start_time);
void curr_depth_status(chessBoard &_cb);
void Show_Searched_Info(chessBoard &_cb);
void reset_pv_line();
int checkmate_score(int ply);

// Move_Generator
void order_generated_moves(MoveList& myMoves, bool pv_moves);
int createMoveOrderList(chessBoard &_cb);
bool is_valid_move(int move, chessBoard _cb);

// Search_Checks
bool ok_to_do_nullmove(chessBoard& _cb);
bool ok_to_fprune(int depth, chessBoard& _cb, MoveList& myMoves, int beta);
int apply_search_extensions(MoveList& myMoves);
bool ok_to_do_LMR(int depth, MoveList& myMoves);
int root_reduction(int depth, int num);
int reduction (int depth, int num);
int MaterialCount(chessBoard& _cb);

// Search
int QuieSearch(chessBoard &_cb, int alpha, int beta, int ply, int __dol);
int AlphaBeta_noPV(chessBoard &_cb, int depth, int alpha, int beta, int ply);




inline bool
time_left_for_search()
{ return extra_time_left; }

#endif

