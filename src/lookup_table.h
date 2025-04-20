

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
  extern MaskTable upMasks;
  extern MaskTable downMasks;
  extern MaskTable leftMasks;
  extern MaskTable rightMasks;

  extern MaskTable upRightMasks;
  extern MaskTable upLeftMasks;
  extern MaskTable downRightMasks;
  extern MaskTable downLeftMasks;

  extern MaskTable lineMasks;     // (	 UpMask | 	DownMask | 		LeftMask | 	  RightMask)
  extern MaskTable diagonalMasks; // (UpRightMask | UpLeftMask | DownRightMask | DownLeftMask)

  extern MaskTable rookMasks;
  extern MaskTable bishopMasks;
  extern MaskTable knightMasks;
  extern MaskTable kingMasks;
  extern MaskTable kingOuterMasks;

  extern array<MaskTable, COLOR_NB> pawnMasks;
  extern array<MaskTable, COLOR_NB> pawnCaptureMasks;
  extern array<MaskTable, COLOR_NB> passedPawnMasks;

  extern MaskTable rookStartIndex;
  extern MaskTable bishopStartIndex;

  extern Bitboard *rookMovesLookUp;
  extern Bitboard *bishopMovesLookUp;

  extern MaskTable rookMagics;
  extern ShiftTable rookShifts;
  extern MaskTable bishopMagics;
  extern ShiftTable bishopShifts;

  void
  init();
}

#endif
