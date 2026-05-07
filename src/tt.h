

#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include "types.h"
#include <iostream>
#include <random>
#include <array>

using std::array;

/**
 * Packed TT entry — 16 bytes total.
 *
 * `data` layout (uint64_t):
 *   bits  0..23 → bestMove   (24 bits, matches Move encoding width)
 *   bits 24..31 → depth      (8 bits, unsigned)
 *   bits 32..33 → flag       (2 bits)
 *   bits 34..49 → eval       (16-bit signed, fits VALUE_INF = 16001)
 *   bits 50..63 → reserved   (14 bits — aging, etc.)
 */
class ZobristHashKey
{
  public:
  Bitboard hashValue;
  uint64_t data;

  ZobristHashKey() : hashValue(0), data(0) {}

  inline Move
  bestMove() const noexcept
  { return Move(data & 0xFFFFFFULL); }

  inline Depth
  depth() const noexcept
  { return Depth((data >> 24) & 0xFFULL); }

  inline Flag
  flag() const noexcept
  { return Flag((data >> 32) & 0x3ULL); }

  inline Score
  eval() const noexcept
  {
    // sign-extend the 16-bit eval field
    int16_t v = int16_t((data >> 34) & 0xFFFFULL);
    return Score(v);
  }

  inline void
  pack(Score eval, Depth depth, Flag flag, Move bestMove) noexcept
  {
    data = (uint64_t(bestMove) & 0xFFFFFFULL)
         | ((uint64_t(depth) & 0xFFULL) << 24)
         | ((uint64_t(flag)  & 0x3ULL) << 32)
         | ((uint64_t(uint16_t(int16_t(eval))) & 0xFFFFULL) << 34);
  }

  void
  show() const noexcept
  {
    std::cout
      << "Key = " << hashValue << '\n'
      << "Depth = " << depth() << '\n'
      << "Eval = " << eval() << '\n'
      << "Flag = " << int(flag()) << '\n'
      << "BestMove = " << bestMove() << std::endl;
  }
};

class TranspositionTable
{
  /**
   * 0 ->  66 MB tableSize
   * 1 -> 686 MB tableSize
  */
  array<uint64_t, 2> ttSizes = { 2189477ULL, 22508861ULL };

  size_t TT_SIZE = 0;

  ZobristHashKey *ttPrimary;
  ZobristHashKey *ttSecondary;

  array<uint64_t, HASH_INDEXES_SIZE> hashIndex;

  void getRandomKeys() noexcept;

  void allocateTables();

  void freeTables();

  public:
  // Initialize Transposition Table
  TranspositionTable() { }

  TranspositionTable(int preset)
  { resize(preset); }

  void
  resize(int preset = 0);

  std::string
  size() const noexcept;

  void
  clear() noexcept;

  uint64_t
  hashKey(int pos) const noexcept
  { return hashIndex[pos]; }

  uint64_t
  hashKeyUpdate(int piece, int pos) const noexcept;

  void
  recordPosition(uint64_t hashValue, Depth depth, Score eval, Flag flag, Move bestMove) noexcept;

  int
  lookupPosition(uint64_t hashValue, Depth depth, Score alpha, Score beta, Move& outMove) const noexcept;
};


extern TranspositionTable tt;

#endif
