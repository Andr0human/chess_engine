

#include "tt.h"
#include "search_utils.h"   // isMateScore

using std::string;
using std::to_string;

TranspositionTable tt;

// --- Mate-score <-> TT conversion -------------------------------------------
//
// Mate scores are encoded root-relative (checkmateScore = -VALUE_MATE + 20*ply),
// so the value carries the *absolute* ply at which the mate lands. A TT entry,
// though, is shared by every path that reaches the position, and those paths sit
// at different plies: an entry written at ply 8 and read at ply 3 would report a
// mate 5 plies further out than it is, and could hand back a bogus cutoff.
//
// So mates are stored *node-relative* (distance from the entry's own node) and
// converted back on probe. Non-mate scores are path-independent and pass through
// untouched — which is what keeps this change inert outside the mate window.
//
// Range: the largest magnitude ever stored is VALUE_MATE + 20*MAX_PLY = 17000,
// comfortably inside the 16-bit signed eval field (see ZobristHashKey::pack).
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
    (uint64_t hashValue, Depth depth, Ply ply, Score eval, Flag flag, Move bestMove) noexcept
{
  // Store mate scores as a distance from *this* node, not from the root.
  const Score storedEval = valueToTt(eval, ply);

  const auto addEntry = [&] (ZobristHashKey& key)
  {
    key.hashValue = hashValue;
    key.pack(storedEval, depth, flag, bestMove);
  };

  size_t index = hashValue % TT_SIZE;

  if (depth > ttPrimary[index].depth())
    addEntry(ttPrimary[index]);

  addEntry(ttSecondary[index]);
}

int
TranspositionTable::lookupPosition
  (uint64_t hashValue, Depth depth, Ply ply, Score alpha, Score beta, Move& outMove, bool& ttHit) const noexcept
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
      // Back to root-relative *before* the bound tests — alpha and beta are
      // root-relative, so comparing a node-relative mate against them would
      // cut off on the wrong distance.
      Score eval = valueFromTt(key.eval(), ply);
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
TranspositionTable::probePvMove(uint64_t hashValue, Depth minDepth) const noexcept
{
  // Only an exact-bound entry proved its move to be *the* move. HASH_ALPHA
  // entries store whatever survived a node where everything failed low, and
  // HASH_BETA entries store a refutation that is only known to be good enough,
  // not best. Handing either to the PV printer manufactures analysis: it takes
  // one bogus link for every probe past it to be asking about a position that
  // was never on the line at all.
  const auto probe = [&] (const ZobristHashKey& key) -> Move
  {
    if (key.hashValue != hashValue)
      return NULL_MOVE;
    if (key.flag() != Flag::HASH_EXACT or key.depth() < minDepth)
      return NULL_MOVE;
    return key.bestMove();
  };

  size_t index = hashValue % TT_SIZE;

  Move move = probe(ttPrimary[index]);
  if (move != NULL_MOVE)
    return move;

  return probe(ttSecondary[index]);
}

void
TranspositionTable::clear() noexcept
{
  for (size_t i = 0; i < TT_SIZE; i++)
    ttPrimary[i].hashValue = ttSecondary[i].hashValue = 0;
}
