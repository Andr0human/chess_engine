

#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include "types.h"
#include <iostream>
#include <random>
#include <array>

using std::array;

class ZobristHashKey
{
  public:
  Bitboard hashValue;
  Score         eval;
  uint32_t depthFlag;

  ZobristHashKey() : hashValue(0) {}

  inline Depth
  depth() const noexcept
  { return depthFlag >> 2; }

  inline Flag
  flag() const noexcept
  { return Flag(depthFlag & 3); }

  void
  show() const noexcept
  {
    std::cout 
      << "Key = " << hashValue << '\n'
      << "Depth = " << depth() << '\n'
      << "Eval = " << eval << '\n'
      << "Flag = " << int(flag()) << std::endl;
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
  recordPosition(uint64_t hashValue, Depth depth, Score eval, Flag flag) noexcept;
  
  int
  lookupPosition(uint64_t hashValue, Depth depth, Score alpha, Score beta) const noexcept;
};


extern TranspositionTable tt;

#endif
