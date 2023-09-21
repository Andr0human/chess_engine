

#ifndef ATTACKS_H
#define ATTACKS_H

#include "types.h"
#include "lookup_table.h"


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



#endif

