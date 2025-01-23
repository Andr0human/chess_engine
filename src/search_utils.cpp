
#include "search_utils.h"
#include "movegen.h"

Move pvArray[MAX_PV_ARRAY_SIZE];
const string StartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";


void
movcpy(Move* pTarget, const Move* pSource, int n)
{ while (n-- && (*pTarget++ = *pSource++)); }

void
ResetPvLine()
{
  for (size_t i = 0; i < MAX_PV_ARRAY_SIZE; i++)
    pvArray[i] = NULL_MOVE;
}

Score
CheckmateScore(Ply ply)
{ return -VALUE_MATE + (20 * ply); }

bool
FutilityOk(Move move)
{
  return !(is_type<CAPTURES>(move) or is_type<CHECK>(move) or is_type<PROMOTION>(move));
}

bool
LmrOk(Move move, Depth depth, size_t moveNo)
{
  if ((depth < 2) or (moveNo < LMR_LIMIT) or InterestingMove(move))
    return false;

  return true;
}

bool
InterestingMove(Move move)
{
  if (is_type<CAPTURES>(move) or is_type<CHECK>(move))
    return true;

  if (is_type<CASTLING>(move) or is_type<PROMOTION>(move))
    return true;
  
  return false;
}

int
RootReduction(Depth depth, size_t moveNo)
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
Reduction (Depth depth, size_t moveNo)
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
SearchExtension(const ChessBoard& pos, const MoveList& myMoves, int numExtensions)
{
  if (numExtensions >= EXTENSION_LIMIT)
    return 0;
  
  // If king is in check, add 1
  if (myMoves.checkers > 0 and myMoves.size() < 3)
    return 1;

  // if queen trapped and attacked by minor piece, add 1
  if (
    (myMoves.checkers == 0) and
    ((pos.get_piece(pos.color, QUEEN) & myMoves.pinnedPiecesSquares ) == 0) and
    ((pos.get_piece(pos.color, QUEEN) & myMoves.enemyAttackedSquares) != 0) and
    QueenTrapped(pos, myMoves.enemyAttackedSquares)
  ) return 1;
  
  return 0;
}

