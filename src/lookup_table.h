

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
  extern array<MaskTable, COLOR_NB> ruleOfSquares;

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


  // Square-to-square distance tables. 4 KB each, and constexpr rather than
  // built in init(), so they are plain .rodata and carry no dependency on
  // plt::init() having run -- unlike every mask table above.
  namespace detail
  {
    // One walk, two metrics: chebyshev keeps the larger axis gap, manhattan the
    // sum. `chebyshev` picks which, so the two tables cannot drift apart.
    constexpr array<array<uint8_t, SQUARE_NB>, SQUARE_NB>
    makeDistanceTable(bool chebyshev) noexcept
    {
      array<array<uint8_t, SQUARE_NB>, SQUARE_NB> table {};

      for (int s1 = 0; s1 < SQUARE_NB; s1++) {
        for (int s2 = 0; s2 < SQUARE_NB; s2++) {
          const int dRank = __abs((s1 >> 3) - (s2 >> 3));
          const int dFile = __abs((s1 & 7)  - (s2 & 7));

          table[size_t(s1)][size_t(s2)] = uint8_t(
            chebyshev ? (dRank > dFile ? dRank : dFile) : (dRank + dFile));
        }
      }

      return table;
    }
  }

  inline constexpr auto chebyshevTable = detail::makeDistanceTable(true);
  inline constexpr auto manhattanTable = detail::makeDistanceTable(false);

  // King-move distance: chebyshev keeps only the larger of the two axis gaps,
  // so a1-h8 and a1-h1 both read 7. Manhattan keeps the sum, and the pair pins
  // down both gaps (manhattan - chebyshev is the smaller one), which is what
  // tells a diagonal approach apart from a straight one.
  inline int
  chebyshevDistance(Square s1, Square s2) noexcept
  { return chebyshevTable[size_t(s1)][size_t(s2)]; }

  inline int
  manhattanDistance(Square s1, Square s2) noexcept
  { return manhattanTable[size_t(s1)][size_t(s2)]; }
}

#endif
