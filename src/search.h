
#ifndef SEARCH_H
#define SEARCH_H


#include "bitboard.h"
#include "movegen.h"
#include "search_utils.h"
#include "evaluation.h"


// Variables

extern MoveType pvArray[(maxPly * maxPly + maxPly) / 2];
extern MoveType thread_array[maxThreadCount][(maxPly * maxPly) / 2];
extern const string startFen;

// Utility

void movcpy (MoveType* pTarget, const MoveType* pSource, int n);
void curr_depth_status(const chessBoard &_cb);
void Show_Searched_Info(chessBoard &_cb);
void reset_pv_line();
int checkmate_score(int ply);

// Move_Generator
void order_generated_moves(MoveList& myMoves, bool pv_moves);
int createMoveOrderList(chessBoard &_cb);
bool is_valid_move(MoveType move, chessBoard _cb);

// Search_Checks
// bool ok_to_do_nullmove(chessBoard& _cb);
// bool ok_to_fprune(int depth, chessBoard& _cb, MoveList& myMoves, int beta);
// int apply_search_extensions(MoveList& myMoves);
bool ok_to_do_LMR(int depth, MoveList& myMoves);
int root_reduction(int depth, int num);
int reduction (int depth, int num);
int MaterialCount(chessBoard& _cb);

// Search
int QuieSearch(chessBoard &_cb, int alpha, int beta, int ply, int __dol);
int AlphaBeta_noPV(chessBoard &_cb, int depth, int alpha, int beta, int ply);


#endif

