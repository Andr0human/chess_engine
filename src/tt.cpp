

#include "tt.h"

TranspositionTable TT;

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
TranspositionTable::GetRandomKeys() noexcept
{
  std::mt19937_64 rng(VALUE_TRANSPOSITION_TABLE_SEED);
  for (int i = 0; i < HASH_INDEXES_SIZE; i++)
    HashIndex[i] = rng();
}

void
TranspositionTable::FreeTables()
{
  if (TT_SIZE == 0)
    return;

  delete[] ttPrimary;
  delete[] ttSecondary;
}

void
TranspositionTable::AllocateTables()
{
  ttPrimary   = new ZobristHashKey[TT_SIZE]();
  ttSecondary = new ZobristHashKey[TT_SIZE]();
}

void
TranspositionTable::resize(int preset)
{
  GetRandomKeys();
  FreeTables();
  TT_SIZE = tt_sizes[preset];
  AllocateTables();
}

std::string
TranspositionTable::size() const noexcept
{
  uint64_t table_size = sizeof(ZobristHashKey) * TT_SIZE * 2;

  uint64_t KB = 1024, MB = KB * KB, GB = MB * KB;

  if (table_size < MB)
    return std::to_string(table_size / KB) + std::string(" KB.");

  if (table_size < GB)
    return std::to_string(table_size / MB) + std::string(" MB.");

  return std::to_string(static_cast<float>(table_size) / static_cast<float>(GB)) + std::string(" GB.");
}

uint64_t
TranspositionTable::HashkeyUpdate
  (int piece, int __pos) const noexcept
{
  int offset = 85;
  int color = piece >> 3;
  piece = (piece & 7) - 1;

  return HashIndex[ offset + __pos
      + 64 * (piece + (6 * color)) ];
}

void
TranspositionTable::RecordPosition
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
TranspositionTable::LookupPosition
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
TranspositionTable::Clear() noexcept
{
  for (size_t i = 0; i < TT_SIZE; i++)
    ttPrimary[i].hashValue = ttSecondary[i].hashValue = 0;
}

