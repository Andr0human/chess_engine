
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
OrderMoves(const ChessBoard& pos, MoveArray& movesArray, MType mTypes, Ply ply, size_t start)
{
  const auto seeComparator = [&pos] (Move move1, Move move2)
  { return SeeScore(pos, move1) > SeeScore(pos, move2); };

  size_t prevS = start;

  if (hasFlag(mTypes, MType::CAPTURES))  start = PrioritizeMoves<MType::CAPTURES>(movesArray, start);
  if (hasFlag(mTypes, MType::PROMOTION)) start = PrioritizeMoves<MType::PROMOTION>(movesArray, start);
  if (hasFlag(mTypes, MType::CHECK))     start = PrioritizeMoves<MType::CHECK   >(movesArray, start);
  if (hasFlag(mTypes, MType::PV))        start = PrioritizeMoves<MType::PV      >(movesArray, start);
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
