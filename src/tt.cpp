

#include "tt.h"

using std::string;
using std::to_string;

TranspositionTable tt;

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
    (uint64_t hashValue, Depth depth, Score eval, Flag flag, Move bestMove) noexcept
{
  const auto addEntry = [&] (ZobristHashKey& key)
  {
    key.hashValue = hashValue;
    key.pack(eval, depth, flag, bestMove);
  };

  size_t index = hashValue % TT_SIZE;

  if (depth > ttPrimary[index].depth())
    addEntry(ttPrimary[index]);

  addEntry(ttSecondary[index]);
}

int
TranspositionTable::lookupPosition
  (uint64_t hashValue, Depth depth, Score alpha, Score beta, Move& outMove, bool& ttHit) const noexcept
{
  const auto probe = [&] (const ZobristHashKey &key) -> int
  {
    if (key.hashValue != hashValue)
      return VALUE_UNKNOWN;

    ttHit = true;

    // Hash match — surface the stored move for ordering, even when the
    // entry's depth is too shallow to produce a cutoff.
    if (outMove == NULL_MOVE)
      outMove = key.bestMove();

    if (key.depth() >= depth)
    {
      Flag flag = key.flag();
      Score eval = key.eval();
      if (flag == Flag::HASH_EXACT) return eval;
      if (flag == Flag::HASH_ALPHA and eval <= alpha) return alpha;
      if (flag == Flag::HASH_BETA  and eval >= beta ) return beta;
    }

    return VALUE_UNKNOWN;
  };

  outMove = NULL_MOVE;
  ttHit = false;

  size_t index = hashValue % TT_SIZE;

  int res = probe(ttPrimary[index]);
  if (res != VALUE_UNKNOWN) return res;

  return probe(ttSecondary[index]);
}

Move
TranspositionTable::probeMove(uint64_t hashValue) const noexcept
{
  size_t index = hashValue % TT_SIZE;

  if (ttPrimary[index].hashValue == hashValue)
    return ttPrimary[index].bestMove();

  if (ttSecondary[index].hashValue == hashValue)
    return ttSecondary[index].bestMove();

  return NULL_MOVE;
}

void
TranspositionTable::clear() noexcept
{
  for (size_t i = 0; i < TT_SIZE; i++)
    ttPrimary[i].hashValue = ttSecondary[i].hashValue = 0;
}
