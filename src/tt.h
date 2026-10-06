

#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include "types.h"
#include <iostream>
#include <random>
#include <array>

using std::array;

/**
 * Packed TT entry, 16 bytes.
 *
 * `data` layout (uint64_t):
 *   bits  0..23: bestMove (24 bits, same width as Move)
 *   bits 24..31: depth    (8 bits, unsigned)
 *   bits 32..33: flag     (2 bits)
 *   bits 34..49: eval     (16-bit signed, fits VALUE_INF = 16001)
 *   bits 50..63: unused   (14 bits, free for aging etc.)
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
  // Number of entries in each table. Always a power of two, so ttMask can be
  // used instead of modulo when calculating the table index.
  size_t ttSize = 0;

  // ttSize - 1. Zero while nothing is allocated, and a probe would then read
  // through a null pointer, so callers check USE_TT first.
  size_t ttMask = 0;

  ZobristHashKey* ttPrimary   = nullptr;
  ZobristHashKey* ttSecondary = nullptr;

  array<uint64_t, HASH_INDEXES_SIZE> hashIndex;

  void allocateTables();

  void freeTables();

  // Checks one entry. lookupPosition() and lookupQuiescence() both use it, so
  // their bound tests are the same. They differ only in which tiers they read.
  int
  probeEntry(const ZobristHashKey& key, uint64_t hashValue, Depth depth, Ply ply,
             Score alpha, Score beta, Move& outMove, bool& ttHit) const noexcept;

  public:
  TranspositionTable() { }

  // Initialize the Zobrist keys. This is needed even when the transposition
  // table is disabled because the keys are also used for position hashing.
  void getRandomKeys() noexcept;

  explicit TranspositionTable(size_t mb)
  { resize(mb); }

  // Resize the tables using the configured minimum and maximum sizes.
  // Existing entries are discarded and the Zobrist keys are regenerated.
  void
  resize(size_t mb = TT_DEFAULT_MB);

  std::string
  size() const noexcept;

  void
  clear() noexcept;

  uint64_t
  hashKey(int pos) const noexcept
  { return hashIndex[pos]; }

  uint64_t
  hashKeyUpdate(int piece, int pos) const noexcept;

  // Store a position in the transposition table. ply is used to convert
  // root-relative mate scores to node-relative scores.
  void
  recordPosition(uint64_t hashValue, Depth depth, Ply ply, Score eval, Flag flag, Move bestMove) noexcept;

  // Store a quiescence result at depth 0, in the primary tier only.
  // recordPosition() always writes the secondary tier, and there are many more
  // q-nodes than main-search nodes, so q-entries would push the main search's
  // entries out of it. The primary tier keeps the deeper entry, so a q-entry
  // only goes into a slot that is empty or already holds a depth-0 entry.
  void
  recordQuiescence(uint64_t hashValue, Ply ply, Score eval, Flag flag, Move bestMove) noexcept;

  int
  lookupPosition(uint64_t hashValue, Depth depth, Ply ply, Score alpha, Score beta, Move& outMove, bool& ttHit) const noexcept;

  // Probe for a quiescence node: depth 0, primary tier only.
  //
  // Most q-probes miss, and a miss that reads both tiers costs two likely cache
  // misses. The entries a q-node can cut on are in the primary tier anyway:
  // recordQuiescence() only writes there, and it holds the deep main-search
  // entries. Probing the secondary tier as well was tested and didn't help.
  //
  // It returns no hash move, because qsearch doesn't use one for ordering.
  Score
  lookupQuiescence(uint64_t hashValue, Ply ply, Score alpha, Score beta, bool& ttHit) const noexcept;

  // Return a stored move suitable for extending the displayed PV.
  // Returns NULL_MOVE unless the entry is an exact result and was searched
  // to at least minDepth.
  Move
  probePvMove(uint64_t hashValue, Depth minDepth) const noexcept;
};

extern TranspositionTable tt;

#endif
