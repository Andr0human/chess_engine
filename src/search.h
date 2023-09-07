
#ifndef SEARCH_H
#define SEARCH_H


#include "bitboard.h"
#include "movegen.h"
#include "search_utils.h"
#include "evaluation.h"


// Variables

constexpr size_t pvArraySize = (MAX_PLY * (MAX_PLY + 1)) / 2;

extern Move pvArray[pvArraySize];
extern Move thread_array[MAX_THREADS][pvArraySize];
extern const string StartFen;

// Utility

void movcpy (Move* pTarget, const Move* pSource, int n);
void ResetPvLine();
Score CheckmateScore(int ply);

// Move_Generator
void ReorderGeneratedMoves(MoveList& myMoves, bool pv_moves);
int createMoveOrderList(ChessBoard& pos);
bool IsValidMove(Move move, ChessBoard pos);

// Search_Checks
// bool ok_to_do_nullmove(ChessBoard& _cb);
// bool ok_to_fprune(Depth depth, ChessBoard& _cb, MoveList& myMoves, Score beta);
bool OkToDoLMR(Depth depth, MoveList& myMoves);
int RootReduction(Depth depth, int num);
int Reduction (Depth depth, int num);
int MaterialCount(ChessBoard& pos);

// Search
Score QuieSearch(ChessBoard& _cb, Score alpha, Score beta, int ply, int __dol);
Score AlphaBetaNonPV(ChessBoard& _cb, Depth depth, Score alpha, Score beta, int ply);


#endif

