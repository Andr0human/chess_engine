
#include "search.h"
#include "move_utils.h"
#include <algorithm>

template <>
bool
is_type<PV_MOVE>(Move m)
{ return info.IsPartOfPV(m); }

SearchData info;

#ifndef MOVE_REORDERING


template <int flag>
static size_t
PrioritizeMoves(MoveArray& myMoves, size_t start)
{
  for (size_t i = start; i < myMoves.size(); i++)
  {
    if (is_type<flag>(myMoves[i]))
      std::swap(myMoves[i], myMoves[start++]);
  }
  return start;
}

template <Sorts sortType>
size_t
OrderMoves(const ChessBoard& pos, MoveArray& movesArray, size_t start)
{
  const auto seeComparator = [&pos] (Move move1, Move move2)
  { return SeeScore(pos, move1) > SeeScore(pos, move2); };

  size_t prevS = start;
  size_t end = movesArray.size();

  if (sortType & Sorts::CAPTURES)
  {
    end = PrioritizeMoves<CAPTURES>(movesArray, start);
    start = end;
  }

  if (sortType & Sorts::PROMOTIONS)
  {
    end = PrioritizeMoves<PROMOTION>(movesArray, start);
    start = end;
  }

  if (sortType & Sorts::CHECKS)
  {
    end = PrioritizeMoves<CHECK>(movesArray, start);
    start = end;
  }

  if (sortType & Sorts::PV)
  {
    end = PrioritizeMoves<PV_MOVE>(movesArray, start);
    start = end;
  }

  if (sortType & Sorts::QUIET)
    end = movesArray.size();

  std::sort(movesArray.begin() + prevS, movesArray.begin() + end, seeComparator);

  return end;
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
    (is_type<CAPTURES>(move) and fpt == NONE) ? pieceValues[PAWN] : pieceValues[fpt];

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


template size_t OrderMoves<Sorts::CAPTURES>(const ChessBoard&, MoveArray&, size_t);
template size_t OrderMoves<Sorts::PROMOTIONS>(const ChessBoard&, MoveArray&, size_t);
template size_t OrderMoves<Sorts::CHECKS>(const ChessBoard&, MoveArray&, size_t);
template size_t OrderMoves<Sorts::PV>(const ChessBoard&, MoveArray&, size_t);
template size_t OrderMoves<Sorts::QUIET>(const ChessBoard&, MoveArray&, size_t);
template size_t OrderMoves<Sorts::CAPTURES | Sorts::PROMOTIONS>(const ChessBoard&, MoveArray&, size_t);
template size_t OrderMoves<Sorts::CAPTURES | Sorts::PROMOTIONS | Sorts::CHECKS>(const ChessBoard&, MoveArray&, size_t);
template size_t OrderMoves<Sorts::CAPTURES | Sorts::CHECKS>(const ChessBoard&, MoveArray&, size_t);

#endif
