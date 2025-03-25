#ifndef SEARCH_UTILS_H
#define SEARCH_UTILS_H

#include "movelist.h"


extern Move pvArray[MAX_PV_ARRAY_SIZE];
extern array<Varray<Move, 2>, MAX_PLY> killerMoves;
extern const string StartFen;


void
movcpy(Move* pTarget, const Move* pSource, int n);

void
ResetPvLine();

Score
CheckmateScore(Ply ply);

template <MType mt>
inline bool is_type(Move m)
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
RootReduction(Depth depth, size_t moveNo);

int
Reduction (Depth depth, size_t moveNo);

int
Reduction2(const ChessBoard& pos, Move move, Depth depth, size_t moveNo);

bool
InterestingMove(Move move);

bool
LmrOk(Move move, Depth depth, size_t moveNo);

int
SearchExtension(
  const ChessBoard& pos,
  const MoveList& myMoves,
  int numExtensions,
  Depth depth
);


Score
SeeScore(const ChessBoard& pos, Move move);

#endif
