
#ifndef SEARCH_H
#define SEARCH_H


#include "bitboard.h"
#include "movegen.h"
#include "search_utils.h"
#include "evaluation.h"


// Variables

extern Move pvArray[MAX_PV_ARRAY_SIZE];
extern Move thread_array[MAX_THREADS][MAX_PV_ARRAY_SIZE];
extern const string StartFen;

// Utility

void movcpy (Move* pTarget, const Move* pSource, int n);
void ResetPvLine();
Score CheckmateScore(Ply ply);



// Search_Checks
// bool ok_to_do_nullmove(ChessBoard& _cb);
// bool ok_to_fprune(Depth depth, ChessBoard& _cb, MoveList& myMoves, Score beta);
bool LmrOk(Move move, Depth depth, size_t move_no);
bool InterestingMove(Move move);

int Reduction(Depth depth, size_t move_no);
int RootReduction(Depth depth, size_t move_no);
int SearchExtension(const MoveList& myMoves, int numExtensions);


void OrderMoves(MoveList& myMoves, bool pv_moves, bool check_moves);


#endif

