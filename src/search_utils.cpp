
#include "search_utils.h"
#include "movegen.h"

Move pvArray[MAX_PV_ARRAY_SIZE];
array<Varray<Move, 2>, MAX_PLY> killerMoves;
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
LmrOk(Move move, Depth depth, size_t moveNo)
{
  if ((depth < 2) or (moveNo < LMR_LIMIT) or InterestingMove(move))
    return false;

  return true;
}

bool
InterestingMove(Move move)
{
  if (is_type<MType::CAPTURES >(move)
   or is_type<MType::PROMOTION>(move)
   or is_type<MType::CHECK    >(move)
  ) return true;

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
Reduction2(const ChessBoard& pos, Move move, Depth depth, size_t moveNo)
{
  int R = 0;

  if ((depth < 2)
   or (moveNo < LMR_LIMIT)
   or is_type<MType::PROMOTION>(move)
   or is_type<MType::CHECK>(move)
  ) return R;

  if (is_type<MType::CAPTURES>(move)) {
    Score seeScore = SeeScore(pos, move);

    if (seeScore >= -2 * PawnValueMg)
      return 0;

    return depth / 4;
  }

  if (depth < 4)
    R = move < 9 ? 0 : 1; 
  else if (depth < 7)
    R = move < 9 ? 1 : 2;
  else
    R = (move < 12) ? (1) : ((moveNo < 24) ? 2 : 3);

  return R;
}

int
SearchExtension(
  const ChessBoard& pos,
  const MoveList& myMoves,
  int numExtensions,
  Depth depth
)
{
  const size_t moveCount = myMoves.countMoves();
  if (numExtensions >= EXTENSION_LIMIT)
    return 0;

  // If king is in check, add 1
  if (myMoves.checkers > 0 and moveCount < 3)
    return 1;

  // if queen trapped and attacked by minor piece, add 1
  if (
    (depth == 1) and
    (myMoves.checkers == 0) and
    PieceTrapped(pos, myMoves.enemyAttackedSquares)
  ) return 1;
  
  return 0;
}


Score
See(const ChessBoard& pos, Square square, Color side, PieceType capturedPiece, Bitboard removedPieces)
{
  const array<Score, ALL> pieceValues = { 0, 100, 320, 300, 530, 910, 3200 };

  Score value = 0;
  Square sq = GetSmallestAttacker(pos, square, side, removedPieces);

  if (sq == SQUARE_NB)
    return value;

  PieceType attacker = type_of(pos.PieceOnSquare(sq));
  const auto seeScore = See(pos, square, ~side, attacker, removedPieces | (1ULL << sq));

  value = std::max(0, pieceValues[capturedPiece] - seeScore);
  return value;
}

Score
SeeScore(const ChessBoard& pos, Move move)
{
  const array<Score, ALL> pieceValues = { 0, 100, 320, 300, 530, 910, 3200 };
  const Square fp =   to_sq(move);
  const Square ip = from_sq(move);
  const PieceType fpt = PieceType((move >> 15) & 7);

  const Color side = ~pos.color;
  const Score initialValue =
    (is_type<MType::CAPTURES>(move) and fpt == NONE) ? pieceValues[PAWN] : pieceValues[fpt];

  Bitboard removedPieces = 1ULL << ip;
  PieceType pieceOnSquare = type_of(pos.PieceOnSquare(ip));

  Score seeScore = initialValue - See(pos, fp, side, pieceOnSquare, removedPieces);
  return seeScore;
}
