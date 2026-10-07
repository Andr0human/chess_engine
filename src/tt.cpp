

#include "tt.h"
#include "base_utils.h"     // msb
#include "search_utils.h"   // isMateScore

#include <algorithm>

using std::string;
using std::to_string;

TranspositionTable tt;

// --- Mate scores in the TT ---------------------------------------------------
//
// Mate scores count plies from the root (checkmateScore = -VALUE_MATE + 20*ply).
// A TT entry is shared by every path that reaches the position, and those paths
// are at different plies. An entry written at ply 8 and read at ply 3 would put
// the mate 5 plies too far away, and could give a wrong cutoff.
//
// So mate scores are stored as a distance from the entry's own node and
// converted back on probe. Other scores don't depend on the path and are
// stored as they are.
//
// The largest stored value is VALUE_MATE + 20*MAX_PLY = 17000, which fits the
// 16-bit eval field (see ZobristHashKey::pack).
static Score
valueToTt(Score eval, Ply ply) noexcept
{
  if (!isMateScore(eval))
    return eval;
  return eval > 0 ? Score(eval + 20 * ply) : Score(eval - 20 * ply);
}

static Score
valueFromTt(Score eval, Ply ply) noexcept
{
  if (!isMateScore(eval))
    return eval;
  return eval > 0 ? Score(eval - 20 * ply) : Score(eval + 20 * ply);
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
  delete[] ttPrimary;
  delete[] ttSecondary;
  ttPrimary = ttSecondary = nullptr;
}

void
TranspositionTable::allocateTables()
{
  ttPrimary   = new ZobristHashKey[ttSize]();
  ttSecondary = new ZobristHashKey[ttSize]();
}

void
TranspositionTable::resize(size_t mb)
{
  getRandomKeys();
  freeTables();

  mb = std::clamp(mb, size_t(TT_MIN_MB), size_t(TT_MAX_MB));

  // Round down to the largest power-of-two entry count that fits. msb() gives
  // that for any non-zero value, and the clamp keeps it non-zero (TT_MIN_MB,
  // 1 MB, is already 32768 entries per table).
  const size_t entriesThatFit = (mb << 20) / (2 * sizeof(ZobristHashKey));
  ttSize = msb(entriesThatFit);
  ttMask = ttSize - 1;

  allocateTables();
}

string
TranspositionTable::size() const noexcept
{
  uint64_t tableSize = sizeof(ZobristHashKey) * ttSize * 2;

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
    (uint64_t hashValue, Depth depth, Ply ply, Score eval, Flag flag, Move bestMove) noexcept
{
  // Store mate scores as a distance from this node, not from the root.
  const Score storedEval = valueToTt(eval, ply);

  const auto addEntry = [&] (ZobristHashKey& key)
  {
    key.hashValue = hashValue;
    key.pack(storedEval, depth, flag, bestMove);
  };

  size_t index = hashValue & ttMask;

  if (depth >= ttPrimary[index].depth())
    addEntry(ttPrimary[index]);

  addEntry(ttSecondary[index]);
}

void
TranspositionTable::recordQuiescence
    (uint64_t hashValue, Ply ply, Score eval, Flag flag, Move bestMove) noexcept
{
  const Score storedEval = valueToTt(eval, ply);

  size_t index = hashValue & ttMask;

  // depth() is unsigned, so this only matches empty and depth-0 slots. tt.h
  // explains why only the primary tier is used.
  if (ttPrimary[index].depth() == 0)
  {
    ttPrimary[index].hashValue = hashValue;
    ttPrimary[index].pack(storedEval, 0, flag, bestMove);
  }
}

int
TranspositionTable::probeEntry
  (const ZobristHashKey& key, uint64_t hashValue, Depth depth, Ply ply,
   Score alpha, Score beta, Move& outMove, bool& ttHit) const noexcept
{
  if (key.hashValue != hashValue)
    return VALUE_UNKNOWN;

  ttHit = true;

  // Return the stored move for ordering, even if the entry is too shallow
  // for a cutoff.
  if (outMove == NULL_MOVE)
    outMove = key.bestMove();

  if (key.depth() >= depth)
  {
    Flag flag = key.flag();
    // Convert mate scores back to root-relative before comparing with alpha
    // and beta, which are root-relative.
    Score eval = valueFromTt(key.eval(), ply);
    if (flag == Flag::HASH_EXACT) return eval;
    if (flag == Flag::HASH_ALPHA and eval <= alpha) return alpha;
    if (flag == Flag::HASH_BETA  and eval >= beta ) return beta;
  }

  return VALUE_UNKNOWN;
}

int
TranspositionTable::lookupPosition
  (uint64_t hashValue, Depth depth, Ply ply, Score alpha, Score beta, Move& outMove, bool& ttHit) const noexcept
{
  outMove = NULL_MOVE;
  ttHit = false;

  size_t index = hashValue & ttMask;

  int res = probeEntry(ttPrimary[index], hashValue, depth, ply, alpha, beta, outMove, ttHit);
  if (res != VALUE_UNKNOWN) return res;

  return probeEntry(ttSecondary[index], hashValue, depth, ply, alpha, beta, outMove, ttHit);
}

Score
TranspositionTable::lookupQuiescence
  (uint64_t hashValue, Ply ply, Score alpha, Score beta, bool& ttHit) const noexcept
{
  // probeEntry() sets a move on a hash match. qsearch doesn't need it (see
  // tt.h).
  Move unused = NULL_MOVE;
  ttHit = false;

  size_t index = hashValue & ttMask;

  return probeEntry(ttPrimary[index], hashValue, 0, ply, alpha, beta, unused, ttHit);
}

Move
TranspositionTable::probePvMove(uint64_t hashValue, Depth minDepth) const noexcept
{
  // Only an exact entry's move is known to be the best move. A HASH_ALPHA
  // entry holds whatever was left when every move failed low, and a HASH_BETA
  // entry holds a move that was good enough for a cutoff, not necessarily the
  // best. One wrong move would make every later probe follow positions that
  // were never on the PV.
  const auto probe = [&] (const ZobristHashKey& key) -> Move
  {
    if (key.hashValue != hashValue)
      return NULL_MOVE;
    if (key.flag() != Flag::HASH_EXACT or key.depth() < minDepth)
      return NULL_MOVE;
    return key.bestMove();
  };

  size_t index = hashValue & ttMask;

  Move move = probe(ttPrimary[index]);
  if (move != NULL_MOVE)
    return move;

  return probe(ttSecondary[index]);
}

void
TranspositionTable::clear() noexcept
{
  for (size_t i = 0; i < ttSize; i++)
    ttPrimary[i].hashValue = ttSecondary[i].hashValue = 0;
}
