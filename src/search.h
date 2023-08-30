
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
Score checkmate_score(int ply);

// Move_Generator
void order_generated_moves(MoveList& myMoves, bool pv_moves);
int createMoveOrderList(ChessBoard& pos);
bool is_valid_move(Move move, ChessBoard pos);

// Search_Checks
// bool ok_to_do_nullmove(ChessBoard& _cb);
// bool ok_to_fprune(Depth depth, ChessBoard& _cb, MoveList& myMoves, Score beta);
// int apply_search_extensions(MoveList& myMoves);
bool ok_to_do_LMR(Depth depth, MoveList& myMoves);
int root_reduction(Depth depth, int num);
int reduction (Depth depth, int num);
int MaterialCount(ChessBoard& pos);

// Search
Score QuieSearch(ChessBoard& _cb, Score alpha, Score beta, int ply, int __dol);
Score alphaBeta_noPV(ChessBoard& _cb, Depth depth, Score alpha, Score beta, int ply);


#endif

