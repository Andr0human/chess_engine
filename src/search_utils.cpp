
#include "search_utils.h"
#include "movegen.h"

Move pvArray[MAX_PV_ARRAY_SIZE];
array<Varray<Move, KILLER_ARRAY_SIZE>, MAX_PLY> killerMoves;
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
    slot.clear();
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

  // Use the same gravity update as the bonus, mirrored around zero.
  // The '+' is intentional: as h becomes more negative, the malus decreases
  // and approaches -MAX_HISTORY instead of growing without bound.
  h -= int32_t(malus + int(h) * malus / int(MAX_HISTORY));
}

Score
checkmateScore(Ply ply)
{ return -VALUE_MATE + (20 * ply); }

// Returns true if score represents a forced mate.
// MATE_BOUND matches the mate-score range used by checkmateScore().
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

  if (myMoves.checkers > 0)
    return 1;

  // Queen trapped and attacked by a minor piece.
  if (
    (depth == 1) and
    (myMoves.checkers == 0) and
    pieceTrapped(pos, myMoves.myAttackedSquares, myMoves.enemyAttackedSquares)
  ) return 1;
  
  return 0;
}

