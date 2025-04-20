
#include "search.h"
#include "move_utils.h"
#include <algorithm>

template <>
bool
is_type<MType::PV>(Move m)
{ return info.isPartOfPv(m); }

SearchData info;

#ifndef MOVE_REORDERING


template <MType mt>
static size_t
prioritizeMoves(MoveArray& movesArray, size_t start)
{
  for (size_t i = start; i < movesArray.size(); i++)
  {
    if (is_type<mt>(movesArray[i]))
      std::swap(movesArray[i], movesArray[start++]);
  }
  return start;
}

size_t
orderMoves(const ChessBoard& pos, MoveArray& movesArray, MType mTypes, Ply ply, size_t start)
{
  const auto seeComparator = [&pos] (Move move1, Move move2)
  { return seeScore(pos, move1) > seeScore(pos, move2); };

  size_t prevS = start;

  if (hasFlag(mTypes, MType::CAPTURES))  start = prioritizeMoves<MType::CAPTURES>(movesArray, start);
  if (hasFlag(mTypes, MType::PROMOTION)) start = prioritizeMoves<MType::PROMOTION>(movesArray, start);
  if (hasFlag(mTypes, MType::CHECK))     start = prioritizeMoves<MType::CHECK   >(movesArray, start);
  if (hasFlag(mTypes, MType::PV))        start = prioritizeMoves<MType::PV      >(movesArray, start);
  if (hasFlag(mTypes, MType::KILLER))
  {
    for (size_t i = start; i < movesArray.size(); i++) {
      if (killerMoves[ply].search(movesArray[i]))
        std::swap(movesArray[i], movesArray[start++]);
    }
  }

  if (mTypes != MType::QUIET)
    std::sort(movesArray.begin() + prevS, movesArray.begin() + start, seeComparator);

  return (hasFlag(mTypes, MType::QUIET)) ? movesArray.size() : start;
}

Score
see(const ChessBoard& pos, Square square, Color side, PieceType capturedPiece, Bitboard removedPieces)
{
  const array<Score, ALL> pieceValues = { 0, 100, 320, 300, 530, 910, 3200 };

  Score value = 0;
  Square sq = getSmallestAttacker(pos, square, side, removedPieces);

  if (sq == SQUARE_NB)
    return value;

  PieceType attacker = type_of(pos.pieceOnSquare(sq));
  const auto seeScore = see(pos, square, ~side, attacker, removedPieces | (1ULL << sq));

  value = std::max(0, pieceValues[capturedPiece] - seeScore);
  return value;
}

Score
seeScore(const ChessBoard& pos, Move move)
{
  const array<Score, ALL> pieceValues = { 0, 100, 320, 300, 530, 910, 3200 };
  const Square fp =   to_sq(move);
  const Square ip = from_sq(move);
  const PieceType fpt = PieceType((move >> 15) & 7);

  const Color side = ~pos.color;
  const Score initialValue =
    (is_type<MType::CAPTURES>(move) and fpt == NONE) ? pieceValues[PAWN] : pieceValues[fpt];

  Bitboard removedPieces = 1ULL << ip;
  PieceType pieceOnSquare = type_of(pos.pieceOnSquare(ip));

  Score seeScore = initialValue - see(pos, fp, side, pieceOnSquare, removedPieces);
  return seeScore;
}

void
printMovelist(MoveArray myMoves, ChessBoard pos)
{
  using std::setw;

  cout << "MoveCount : " << myMoves.size() << '\n'
       << " | No. |   Move   | Encode-Move | See-Score |" << endl;

  for (size_t i = 0; i < myMoves.size(); i++)
  {
    Move move = myMoves[i];
    string moveString = printMove(move, pos);
    Score score = seeScore(pos, move);

    cout << " | " << setw(3)  << (i + 1)
         << " | " << setw(8)  << moveString
         << " | " << setw(11) << move
         << " | " << setw(9)  << score
         << " |"  << endl;
  }

  cout << endl;
}

#endif
