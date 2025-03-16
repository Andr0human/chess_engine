
#include "search.h"
#include "move_utils.h"
#include <algorithm>

template <>
bool
is_type<MType::PV>(Move m)
{ return info.IsPartOfPV(m); }

SearchData info;

#ifndef MOVE_REORDERING


template <MType mt>
static size_t
PrioritizeMoves(MoveArray& myMoves, size_t start)
{
  for (size_t i = start; i < myMoves.size(); i++)
  {
    if (is_type<mt>(myMoves[i]))
      std::swap(myMoves[i], myMoves[start++]);
  }
  return start;
}

size_t
OrderMoves(const ChessBoard& pos, MoveArray& movesArray, MType mTypes, size_t start)
{
  const auto seeComparator = [&pos] (Move move1, Move move2)
  {
    Score see1 = SeeScore(pos, move1);
    Score see2 = SeeScore(pos, move2);

    int ft1 = (move1 >> 15) & 7;
    int ft2 = (move2 >> 15) & 7;

    if ((see1 == see2) and ft2)
    {
      Weight w1 = (ft1 ? pos.pieceValues[((move1 >> 15) & 7) - 1] : 0) - pos.pieceValues[((move1 >> 12) & 7) - 1];
      Weight w2 = (ft2 ? pos.pieceValues[((move2 >> 15) & 7) - 1] : 0) - pos.pieceValues[((move2 >> 12) & 7) - 1];

      return w1 > w2;
    }

    return see1 > see2;
  };

  size_t prevS = start;

  if (hasFlag(mTypes, MType::CAPTURES))  start = PrioritizeMoves<MType::CAPTURES>(movesArray, start);
  if (hasFlag(mTypes, MType::PROMOTION)) start = PrioritizeMoves<MType::PROMOTION>(movesArray, start);
  if (hasFlag(mTypes, MType::CHECK))     start = PrioritizeMoves<MType::CHECK   >(movesArray, start);
  if (hasFlag(mTypes, MType::PV))        start = PrioritizeMoves<MType::PV      >(movesArray, start);

  if (mTypes != MType::QUIET)
    std::sort(movesArray.begin() + prevS, movesArray.begin() + start, seeComparator);

  return (hasFlag(mTypes, MType::QUIET)) ? movesArray.size() : start;
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

void
PrintMovelist(MoveArray myMoves, ChessBoard pos)
{
  using std::setw;

  cout << "MoveCount : " << myMoves.size() << '\n'
       << " | No. |   Move   | Encode-Move | See-Score |" << endl;

  for (size_t i = 0; i < myMoves.size(); i++)
  {
    Move move = myMoves[i];
    string moveString = PrintMove(move, pos);
    Score seeScore = SeeScore(pos, move);

    cout << " | " << setw(3)  << (i + 1)
         << " | " << setw(8)  << moveString
         << " | " << setw(11) << move
         << " | " << setw(9)  << seeScore
         << " |"  << endl;
  }

  cout << endl;
}

#endif
