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
constexpr bool
is_type(Move m)
{ return (m & (flag << 21)) != 0; }

// template <>
// bool
// is_type<PV_MOVE>(Move m)
// { return info.IsPartOfPV(m); }


int
RootReduction(Depth depth, size_t moveNo);

int
Reduction (Depth depth, size_t moveNo);

bool
InterestingMove(Move move);

bool
LmrOk(Move move, Depth depth, size_t moveNo);

int
SearchExtension(const MoveList& myMoves, int numExtensions);


#endif


