
#include "attacks.h"
#include "types.h"

template <Color cMy>
Bitboard
pawnAttackSquares(const ChessBoard& pos)
{
  Bitboard pawns = pos.piece<cMy, PAWN>();
  const int inc  = 2 * int(cMy) - 1;
  const ShifterFunc shift = cMy == WHITE ? leftShift : rightShift;

  return shift(pawns & RightAttkingPawns, 8 + inc) |
         shift(pawns & LeftAttkingPawns , 8 - inc);
}

// Returns all squares attacked by pawn on index sq
template <>
Bitboard
attackSquares<WHITE, PAWN>(Square sq)
{ return plt::pawnCaptureMasks[WHITE][sq]; }

template <>
Bitboard
attackSquares<BLACK, PAWN>(Square sq)
{ return plt::pawnCaptureMasks[BLACK][sq]; }

// Returns all squares attacked by bishop on index sq
template <>
Bitboard
attackSquares<BISHOP>(Square sq, Bitboard occupied)
{
  uint64_t magic = plt::bishopMagics[sq];
  int      bits  = plt::bishopShifts[sq];
  Bitboard mask  = plt::bishopMasks[sq];
  uint64_t start = plt::bishopStartIndex[sq];

  uint64_t occupancy = (magic * (occupied & mask)) >> bits;
  return plt::bishopMovesLookUp[start + occupancy];
}

// Returns all squares attacked by knight on index sq
template <>
Bitboard
attackSquares<KNIGHT>(Square sq, Bitboard occupied)
{
  return plt::knightMasks[sq] + (occupied - occupied);
}

// Returns all squares attacked by rook on index sq
template <>
Bitboard
attackSquares<ROOK>(Square sq, Bitboard occupied)
{
  uint64_t magic = plt::rookMagics[sq];
  int      bits  = plt::rookShifts[sq];
  Bitboard mask  = plt::rookMasks[sq];
  uint64_t start = plt::rookStartIndex[sq];

  uint64_t occupancy = (magic * (occupied & mask)) >> bits;
  return plt::rookMovesLookUp[start + occupancy];
}

// Returns all squares attacked by queen on index sq
template <>
Bitboard
attackSquares<QUEEN>(Square sq, Bitboard occupied)
{
  return attackSquares<BISHOP>(sq, occupied)
       | attackSquares< ROOK >(sq, occupied);
}

template <>
Bitboard
attackSquares<KING>(Square sq, Bitboard occupied)
{
  return plt::kingMasks[sq] + (occupied - occupied);
}


template Bitboard pawnAttackSquares<WHITE>(const ChessBoard& pos);
template Bitboard pawnAttackSquares<BLACK>(const ChessBoard& pos);
