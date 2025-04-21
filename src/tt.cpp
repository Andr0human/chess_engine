

#include "tt.h"

using std::string;
using std::to_string;

TranspositionTable tt;

//* TODO xorshift for Random HashKey Generation
uint64_t
xorshift64star(void)
{
  /* initial seed must be nonzero, don't use a static variable for the state if multithreaded */
  static uint64_t x = 1237;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  return x * 0x2545F4914F6CDD1DULL;
}

void
TranspositionTable::getRandomKeys() noexcept
{
  std::mt19937_64 rng(VALUE_TRANSPOSITION_TABLE_SEED);
  for (int i = 0; i < HASH_INDEXES_SIZE; i++)
    hashIndex[i] = rng();
}

void
TranspositionTable::freeTables()
{
  if (TT_SIZE == 0)
    return;

  delete[] ttPrimary;
  delete[] ttSecondary;
}

void
TranspositionTable::allocateTables()
{
  ttPrimary   = new ZobristHashKey[TT_SIZE]();
  ttSecondary = new ZobristHashKey[TT_SIZE]();
}

void
TranspositionTable::resize(int preset)
{
  getRandomKeys();
  freeTables();
  TT_SIZE = ttSizes[preset];
  allocateTables();
}

string
TranspositionTable::size() const noexcept
{
  uint64_t tableSize = sizeof(ZobristHashKey) * TT_SIZE * 2;

  uint64_t KB = 1024, MB = KB * KB, GB = MB * KB;

  if (tableSize < MB)
    return to_string(tableSize / KB) + string(" KB.");

  if (tableSize < GB)
    return to_string(tableSize / MB) + string(" MB.");

  return to_string(static_cast<float>(tableSize) / static_cast<float>(GB)) + string(" GB.");
}

uint64_t
TranspositionTable::hashKeyUpdate
  (int piece, int pos) const noexcept
{
  int offset = 85;
  int color = piece >> 3;
  piece = (piece & 7) - 1;

  return hashIndex[ offset + pos
      + 64 * (piece + (6 * color)) ];
}

void
TranspositionTable::recordPosition
    (uint64_t hashValue, Depth depth, Score eval, Flag flag) noexcept
{
  const auto addEntry = [&] (ZobristHashKey& key)
  {
    key.hashValue = hashValue;
    key.eval = eval;
    key.depthFlag = depth << 2 | int(flag);
  };

  size_t index = hashValue % TT_SIZE;

  if (depth > ttPrimary[index].depth())
    addEntry(ttPrimary[index]);

  addEntry(ttSecondary[index]);
}

int
TranspositionTable::lookupPosition
  (uint64_t hashValue, Depth depth, Score alpha, Score beta) const noexcept
{
  const auto lookup = [&] (const ZobristHashKey &key)
  {
    Flag flag = key.flag();

    if (key.hashValue == hashValue and key.depth() > depth) {
      if (flag == Flag::HASH_EXACT) return key.eval;
      if (flag == Flag::HASH_ALPHA and key.eval <= alpha) return alpha;
      if (flag == Flag::HASH_BETA  and key.eval >= beta ) return beta;
    }

    return int(VALUE_UNKNOWN);
  };

  size_t index = hashValue % TT_SIZE;

  int res = lookup(ttPrimary[index]);
  if (res != VALUE_UNKNOWN) return res;

  return lookup(ttSecondary[index]);
}

void
TranspositionTable::clear() noexcept
{
  for (size_t i = 0; i < TT_SIZE; i++)
    ttPrimary[i].hashValue = ttSecondary[i].hashValue = 0;
}
