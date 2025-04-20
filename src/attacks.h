

#ifndef ATTACKS_H
#define ATTACKS_H

#include "types.h"
#include "lookup_table.h"
#include "bitboard.h"

typedef Bitboard (*ShifterFunc)(Bitboard val, int shift);

// Returns and removes the LsbIndex
inline Square
nextSquare(uint64_t& x)
{
  Square res = Square(__builtin_ctzll(x));
  x &= x - 1;
  return res;
}

template <PieceType pt>
Bitboard
attackSquares(Square sq, Bitboard occupied);

template <Color c, PieceType pt>
Bitboard
attackSquares(Square sq);

template <Color cMy>
Bitboard
pawnAttackSquares(const ChessBoard& pos);

#endif

