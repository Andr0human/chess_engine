
#include "search_utils.h"
#include "movegen.h"

Move pvArray[MAX_PV_ARRAY_SIZE];
array<Varray<Move, 2>, MAX_PLY> killerMoves;
array<array<array<int32_t, SQUARE_NB>, SQUARE_NB>, COLOR_NB> historyTable;

void
movcpy(Move* pTarget, const Move* pSource, int n)
{ while (n-- && (*pTarget++ = *pSource++)); }

void
resetPvLine()
{
  for (size_t i = 0; i < MAX_PV_ARRAY_SIZE; i++)
    pvArray[i] = NULL_MOVE;
}

void
clearKillers()
{
  for (auto& slot : killerMoves)
    slot.clearKillerMoves();
}

void
clearHistory()
{
  for (auto& byColor : historyTable)
    for (auto& byFrom : byColor)
      byFrom.fill(0);
}

void
updateHistory(Color c, Move move, Depth depth)
{
  // depth is >= 1 here: alphaBeta hands depth <= 0 to quiescenceSearch before
  // any move is staged, so the bonus is always positive.
  const int bonus = int(depth) * int(depth);
  int32_t& h = historyTable[c][size_t(from_sq(move))][size_t(to_sq(move))];

  // h is bounded by MAX_HISTORY and bonus by (MAX_DEPTH + EXTENSION_LIMIT)^2,
  // so the product stays far inside int32.
  h += int32_t(bonus - int(h) * bonus / int(MAX_HISTORY));
}

void
penalizeHistory(Color c, Move move, Depth depth)
{
  const int malus = int(depth) * int(depth);
  int32_t& h = historyTable[c][size_t(from_sq(move))][size_t(to_sq(move))];

  // The mirror of the bonus is `-= malus + h*malus/MAX`, NOT `-= malus - ...`.
  // Work the sign through: with h negative, `h*malus/MAX` is negative, so the
  // `+` shrinks the decrement as h approaches -MAX_HISTORY (the asymptote the
  // bonus form has at +MAX_HISTORY). Writing `-` instead makes the decrement
  // *grow* the more negative h gets -- an unbounded runaway that eventually
  // overflows int32 and, long before that, stops the ordering key from being
  // comparable between moves. There is no compile error for getting it wrong.
  h -= int32_t(malus + int(h) * malus / int(MAX_HISTORY));
}

Score
checkmateScore(Ply ply)
{ return -VALUE_MATE + (20 * ply); }

// True when `score` encodes a forced mate rather than a normal evaluation.
// The band must match checkmateScore()'s 20-points-per-ply step: the old
// threshold (VALUE_MATE - MAX_PLY = 15960) assumed 1 point per ply and so only
// recognized mates at ply <= 2 — a mate at ply 3 (-15940) fell through and the
// guards built on this predicate were effectively dead. MATE_BOUND = 15000
// covers the full 0..MAX_PLY range.
bool
isMateScore(Score score)
{ return __abs(score) >= int(MATE_BOUND); }

int
nullReduction(Depth depth)
{ return 3 + depth / 4; }

bool
lmrOk(Move move, Depth depth, size_t moveNo)
{
  if ((depth < 2) or (moveNo < LMR_LIMIT) or interestingMove(move))
    return false;

  return true;
}

bool
interestingMove(Move move)
{
  if (is_type<MType::CAPTURES >(move)
   or is_type<MType::PROMOTION>(move)
   or is_type<MType::CHECK    >(move)
  ) return true;

  return false;
}

int
rootReduction(Depth depth, size_t moveNo)
{
  if (depth < 3) return 0;
  if (depth < 6) {
    if (moveNo < 9) return 1;
    // if (num < 12) return 2;
    return 2;
  }
  if (moveNo < 8) return 2;
  // if (num < 15) return 3;
  return 3;
}

int
reduction (Depth depth, size_t moveNo)
{
  if (depth < 2) return 0;
  if ((depth < 4) and (moveNo > 9)) return 1; 

  if (depth < 7) {
    if (moveNo < 9) return 1;
    return 2;
  }

  if (moveNo < 12) return 1;
  if (moveNo < 24) return 2;
  return 3;
}

int
searchExtension(
  const ChessBoard& pos,
  const MoveList& myMoves,
  int numExtensions,
  Depth depth
)
{
  if (numExtensions >= EXTENSION_LIMIT)
    return 0;

  // If king is in check, add 1
  if (myMoves.checkers > 0)
    return 1;

  // if queen trapped and attacked by minor piece, add 1
  if (
    (depth == 1) and
    (myMoves.checkers == 0) and
    pieceTrapped(pos, myMoves.myAttackedSquares, myMoves.enemyAttackedSquares)
  ) return 1;
  
  return 0;
}

