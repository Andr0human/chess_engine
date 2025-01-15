

#ifndef LOOKUP_TABLE_H
#define LOOKUP_TABLE_H

#include "types.h"
#include <array>

using std::array;
using MaskTable  = array<Bitboard, SQUARE_NB>;
using ShiftTable = array<int     , SQUARE_NB>;


#define __abs(x) ((x >= 0) ? (x) : -(x))

namespace plt
{

extern MaskTable    UpMasks;
extern MaskTable  DownMasks;
extern MaskTable  LeftMasks;
extern MaskTable RightMasks;

extern MaskTable   UpRightMasks;
extern MaskTable    UpLeftMasks;
extern MaskTable DownRightMasks;
extern MaskTable  DownLeftMasks;

extern MaskTable     LineMasks;             // (	 UpMask | 	DownMask | 		LeftMask | 	  RightMask)
extern MaskTable DiagonalMasks;             // (UpRightMask | UpLeftMask | DownRightMask | DownLeftMask)

extern MaskTable      RookMasks;
extern MaskTable    BishopMasks;
extern MaskTable    KnightMasks;
extern MaskTable      KingMasks;
extern MaskTable KingOuterMasks;

extern array<MaskTable, COLOR_NB>        PawnMasks;
extern array<MaskTable, COLOR_NB> PawnCaptureMasks;
extern array<MaskTable, COLOR_NB>  PassedPawnMasks;

extern MaskTable   RookStartIndex;
extern MaskTable BishopStartIndex;

extern Bitboard*   RookMovesLookUp;
extern Bitboard* BishopMovesLookUp;

extern  MaskTable   RookMagics;
extern ShiftTable   RookShifts;
extern  MaskTable BishopMagics;
extern ShiftTable BishopShifts;

void
Init();

}


#endif

