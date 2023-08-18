
#ifndef SEARCH_H
#define SEARCH_H


#include "bitboard.h"
#include "movegen.h"
#include "search_utils.h"
#include "evaluation.h"


// Variables

extern Move pvArray[(MAX_PLY * MAX_PLY + MAX_PLY) / 2];
extern Move thread_array[MAX_THREADS][(MAX_PLY * MAX_PLY) / 2];
extern const string StartFen;

// Utility

void movcpy (Move* pTarget, const Move* pSource, int n);
void reset_pv_line();
int checkmate_score(int ply);

// Move_Generator
void order_generated_moves(MoveList& myMoves, bool pv_moves);
int createMoveOrderList(ChessBoard &_cb);
bool is_valid_move(Move move, ChessBoard _cb);

// Search_Checks
// bool ok_to_do_nullmove(ChessBoard& _cb);
// bool ok_to_fprune(int depth, ChessBoard& _cb, MoveList& myMoves, int beta);
// int apply_search_extensions(MoveList& myMoves);
bool ok_to_do_LMR(int depth, MoveList& myMoves);
int root_reduction(int depth, int num);
int reduction (int depth, int num);
int MaterialCount(ChessBoard& _cb);

// Search
int QuieSearch(ChessBoard &_cb, int alpha, int beta, int ply, int __dol);
int AlphaBeta_noPV(ChessBoard &_cb, int depth, int alpha, int beta, int ply);


#endif

