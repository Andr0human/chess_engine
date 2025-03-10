

#ifndef ATTACKS_H
#define ATTACKS_H

#include "types.h"
#include "lookup_table.h"
#include "bitboard.h"

typedef Bitboard (*ShifterFunc)(Bitboard val, int shift);

// Returns and removes the LsbIndex
inline Square
NextSquare(uint64_t& __x)
{
  Square res = Square(__builtin_ctzll(__x));
  __x &= __x - 1;
  return res;
}


template <PieceType pt>
Bitboard
AttackSquares(Square sq, Bitboard occupied);

template <Color c, PieceType pt>
Bitboard
AttackSquares(Square sq);


template <Color c_my>
Bitboard
PawnAttackSquares(const ChessBoard& _cb);

#endif

