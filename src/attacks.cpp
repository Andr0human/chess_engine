
#include "attacks.h"
#include "types.h"

template <Color c_my>
Bitboard
PawnAttackSquares(const ChessBoard& _cb)
{
  Bitboard pawns = _cb.piece<c_my, PAWN>();
  const int inc  = 2 * int(c_my) - 1;
  const ShifterFunc shift = c_my == WHITE ? LeftShift : RightShift;

  return shift(pawns & RightAttkingPawns, 8 + inc) |
         shift(pawns & LeftAttkingPawns , 8 - inc);
}

// Returns all squares attacked by pawn on index sq
template <>
Bitboard
AttackSquares<WHITE, PAWN>(Square sq)
{ return plt::PawnCaptureMasks[WHITE][sq]; }

template <>
Bitboard
AttackSquares<BLACK, PAWN>(Square sq)
{ return plt::PawnCaptureMasks[BLACK][sq]; }

// Returns all squares attacked by bishop on index sq
template <>
Bitboard
AttackSquares<BISHOP>(Square sq, Bitboard occupied)
{
  uint64_t magic = plt::BishopMagics[sq];
  int      bits  = plt::BishopShifts[sq];
  Bitboard mask  = plt::BishopMasks[sq];
  uint64_t start = plt::BishopStartIndex[sq];

  uint64_t occupancy = (magic * (occupied & mask)) >> bits;
  return plt::BishopMovesLookUp[start + occupancy];
}

// Returns all squares attacked by knight on index sq
template <>
Bitboard
AttackSquares<KNIGHT>(Square sq, Bitboard occupied)
{
  return plt::KnightMasks[sq] + (occupied - occupied);
}

// Returns all squares attacked by rook on index sq
template <>
Bitboard
AttackSquares<ROOK>(Square sq, Bitboard occupied)
{
  uint64_t magic = plt::RookMagics[sq];
  int      bits  = plt::RookShifts[sq];
  Bitboard mask  = plt::RookMasks[sq];
  uint64_t start = plt::RookStartIndex[sq];

  uint64_t occupancy = (magic * (occupied & mask)) >> bits;
  return plt::RookMovesLookUp[start + occupancy];
}

// Returns all squares attacked by queen on index sq
template <>
Bitboard
AttackSquares<QUEEN>(Square sq, Bitboard occupied)
{
  return AttackSquares<BISHOP>(sq, occupied)
       | AttackSquares< ROOK >(sq, occupied);
}

template <>
Bitboard
AttackSquares<KING>(Square sq, Bitboard occupied)
{
  return plt::KingMasks[sq] + (occupied - occupied);
}


template Bitboard PawnAttackSquares<WHITE>(const ChessBoard& pos);
template Bitboard PawnAttackSquares<BLACK>(const ChessBoard& pos);
