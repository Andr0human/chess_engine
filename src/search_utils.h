#ifndef SEARCH_UTILS_H
#define SEARCH_UTILS_H

#include "move_utils.h"


extern Move pvArray[MAX_PV_ARRAY_SIZE];
extern const string StartFen;


void
movcpy(Move* pTarget, const Move* pSource, int n);

void
ResetPvLine();

Score
CheckmateScore(Ply ply);

template <int flag>
bool
is_type(Move m)
{ return ((m >> 21) & 7) == flag; }



int
RootReduction(Depth depth, size_t moveNo);

int
Reduction (Depth depth, size_t moveNo);

bool
InterestingMove(Move move);

bool
LmrOk(Move move, Depth depth, size_t moveNo);

int
SearchExtension(const ChessBoard& pos, const MoveList& myMoves, int numExtensions);


#endif


