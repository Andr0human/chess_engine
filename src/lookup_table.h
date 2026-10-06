

#ifndef LOOKUP_TABLE_H
#define LOOKUP_TABLE_H

#include "types.h"
#include <array>

// Slider table indexing. With BMI2 the index is one PEXT instruction, which
// packs the relevant blockers into a dense index with no collisions. Without
// it, magic multiply-shift is used. -march=native turns on BMI2 when the
// build machine has it. Build with -DNO_PEXT to force magics (for A/B runs or
// portable binaries). AMD Excavator, Zen 1 and Zen 2 have BMI2 but PEXT is
// very slow on them, so they keep the magics.
#if defined(__BMI2__) && !defined(NO_PEXT) \
    && !defined(__bdver4__) && !defined(__znver1__) && !defined(__znver2__)
  #include <immintrin.h>
  #define USE_PEXT 1
#else
  #define USE_PEXT 0
#endif

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

  // Index of an occupancy within one square's part of the slider table. The
  // table builder and attackSquares<> both use this, so they always agree.
  inline uint64_t
  sliderIndex(Bitboard occupied, Bitboard mask,
              [[maybe_unused]] uint64_t magic, [[maybe_unused]] int shift) noexcept
  {
#if USE_PEXT
    return _pext_u64(occupied, mask);
#else
    return (magic * (occupied & mask)) >> shift;
#endif
  }

  // Which indexing this binary was built with. Reported in the UCI `id name`
  // line so arena logs show which variant played.
  inline constexpr const char* SLIDER_INDEXING = USE_PEXT ? "pext" : "magic";


  // Square-to-square distance tables, 4 KB each. They are constexpr, not built
  // in init(), so unlike the mask tables above they work before plt::init()
  // runs.
  namespace detail
  {
    // Chebyshev distance is the larger of the rank and file gaps, Manhattan
    // distance is their sum. One function builds both tables.
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

  // King-move distance: the larger of the rank and file gaps, so a1-h8 and
  // a1-h1 are both 7. Manhattan distance is the sum, so manhattan - chebyshev
  // is the smaller gap. Together they tell a diagonal path from a straight one.
  inline int
  chebyshevDistance(Square s1, Square s2) noexcept
  { return chebyshevTable[size_t(s1)][size_t(s2)]; }

  inline int
  manhattanDistance(Square s1, Square s2) noexcept
  { return manhattanTable[size_t(s1)][size_t(s2)]; }
}

#endif
