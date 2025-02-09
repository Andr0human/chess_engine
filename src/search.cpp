
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


void
OrderMoves(const ChessBoard& pos, MoveArray& myMoves, bool pv_moves, bool check_moves)
{
  const auto seeComparator = [&pos] (Move move1, Move move2)
  { return SeeScore(pos, move1) > SeeScore(pos, move2); };

  const size_t capture_end = PrioritizeMoves<CAPTURES>(myMoves, 0);
  const size_t check_end   = check_moves ? PrioritizeMoves<CHECK>(myMoves, capture_end) : capture_end;
  const size_t pv_end      = pv_moves    ? PrioritizeMoves<PV_MOVE>(myMoves, check_end) : check_end;

  std::sort(myMoves.begin()            , myMoves.begin() + capture_end, seeComparator);
  std::sort(myMoves.begin() + check_end, myMoves.begin() +      pv_end, seeComparator);
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
       << " | No. |   Move   | Encode-Move | Priority | See-Score |" << endl;

  for (size_t i = 0; i < myMoves.size(); i++)
  {
    Move move = myMoves[i];
    string moveString = PrintMove(move, pos);
    Score seeScore = SeeScore(pos, move);
    int priority = move >> 24;

    cout << " | " << setw(3)  << (i + 1)
         << " | " << setw(8)  << moveString
         << " | " << setw(11) << move
         << " | " << setw(8)  << priority
         << " | " << setw(9)  << seeScore
         << " |"  << endl;
  }

  cout << endl;
}

#endif
