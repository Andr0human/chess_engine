
#include "search_utils.h"
#include "movegen.h"

Move pvArray[MAX_PV_ARRAY_SIZE];
array<Varray<Move, 2>, MAX_PLY> killerMoves;

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

