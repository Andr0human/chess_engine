#ifndef SEARCH_UTILS_H
#define SEARCH_UTILS_H

#include "movelist.h"


extern Move pvArray[MAX_PV_ARRAY_SIZE];
extern array<Varray<Move, 2>, MAX_PLY> killerMoves;


void
movcpy(Move* pTarget, const Move* pSource, int n);

void
resetPvLine();

Score
checkmateScore(Ply ply);

template <MType mt>
inline bool
is_type(Move m)
{
  if constexpr (mt == MType::CHECK)
    return (m >> 23) & 1;

  if constexpr (mt == MType::QUIET)
    return ((m >> 20) & 7) == 0;

  if constexpr (mt == MType::CAPTURES)
    return (m >> 20) & 1;

  if constexpr (mt == MType::PROMOTION)
    return (m >> 21) & 1;

  return 0;
}

int
rootReduction(Depth depth, size_t moveNo);

int
reduction (Depth depth, size_t moveNo);

bool
interestingMove(Move move);

bool
lmrOk(Move move, Depth depth, size_t moveNo);

int
searchExtension(
  const ChessBoard& pos,
  const MoveList& myMoves,
  int numExtensions,
  Depth depth
);


#endif


