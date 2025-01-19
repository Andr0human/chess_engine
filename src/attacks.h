

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
AttackSquares(Square sq, Bitboard occupied) = delete;

template <Color c, PieceType pt>
Bitboard
AttackSquares(Square sq) = delete;

// Returns all squares attacked by pawn on index sq
template <>
inline Bitboard
AttackSquares<WHITE, PAWN>(Square sq)
{ return plt::PawnCaptureMasks[WHITE][sq]; }

template <>
inline Bitboard
AttackSquares<BLACK, PAWN>(Square sq)
{ return plt::PawnCaptureMasks[BLACK][sq]; }

// Returns all squares attacked by bishop on index sq
template <>
inline Bitboard
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
inline Bitboard
AttackSquares<KNIGHT>(Square sq, Bitboard occupied)
{
    return plt::KnightMasks[sq] + (occupied - occupied);
}

// Returns all squares attacked by rook on index sq
template <>
inline Bitboard
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
inline Bitboard
AttackSquares<QUEEN>(Square sq, Bitboard occupied)
{
    return AttackSquares<BISHOP>(sq, occupied)
         | AttackSquares< ROOK >(sq, occupied);
}

template <>
inline Bitboard
AttackSquares<KING>(Square sq, Bitboard occupied)
{
    return plt::KingMasks[sq] + (occupied - occupied);
}

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

/**
 * @brief Checks if the king of the active side is in check.
 * 
 * @param ChessBoard position
**/
template <Color c_my>
bool
InCheck(const ChessBoard& _cb)
{
    constexpr Color c_emy = ~c_my;

    Square k_sq = SquareNo( _cb.piece<c_my, KING>() );
    Bitboard occupied = _cb.All();

    return (AttackSquares<ROOK>(k_sq, occupied) & (_cb.piece<c_emy, ROOK  >() | _cb.piece<c_emy, QUEEN>()))
      or (AttackSquares<BISHOP>(k_sq, occupied) & (_cb.piece<c_emy, BISHOP>() | _cb.piece<c_emy, QUEEN>()))
      or (AttackSquares<KNIGHT>(k_sq, occupied) &  _cb.piece<c_emy, KNIGHT>())
      or (plt::PawnCaptureMasks[c_my][k_sq]   &  _cb.piece<c_emy, PAWN  >());
}


#endif

