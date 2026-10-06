
#include "search.h"
#include "move_utils.h"
#include <algorithm>

template <>
bool
is_type<MType::PV>(Move m)
{ return info.isPartOfPv(m); }

SearchData info;

std::atomic<bool> searchStop{false};

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

// Sort the QUIET stage by history score, highest first.
//
// Scores are looked up once into an array, as in orderCaptures. The stage has
// about 26 moves, and a comparator would repeat the lookup about 2n*log(n)
// times. The insertion sort is stable, so moves with equal scores (most are 0)
// keep their generation order.
static void
sortByHistory(Color color, MoveArray& movesArray, size_t start)
{
  const size_t n = movesArray.size();
  if (n - start < 2)
    return;

  array<int32_t, MAX_MOVES> scores;
  for (size_t i = start; i < n; i++)
    scores[i] = historyScore(color, movesArray[i]);

  for (size_t i = start + 1; i < n; i++)
  {
    const Move    move  = movesArray[i];
    const int32_t score = scores[i];
    size_t j = i;

    while (j > start and scores[j - 1] < score)
    {
      movesArray[j] = movesArray[j - 1];
      scores[j]     = scores[j - 1];
      --j;
    }

    movesArray[j] = move;
    scores[j]     = score;
  }
}

size_t
orderMoves(const ChessBoard& pos, MoveArray& movesArray, MType mTypes, Ply ply, size_t start, bool useHistory)
{
  const auto seeComparator = [&pos] (Move move1, Move move2)
  { return seeScore(pos, move1) > seeScore(pos, move2); };

  size_t prevS = start;

  if (hasFlag(mTypes, MType::CAPTURES))  start = prioritizeMoves<MType::CAPTURES>(movesArray, start);
  if (hasFlag(mTypes, MType::PROMOTION)) start = prioritizeMoves<MType::PROMOTION>(movesArray, start);
  if (hasFlag(mTypes, MType::CHECK))     start = prioritizeMoves<MType::CHECK   >(movesArray, start);
  if (hasFlag(mTypes, MType::PV))        start = prioritizeMoves<MType::PV      >(movesArray, start);

  // SEE-sort the tactical band (captures/promotions/checks/PV) before placing
  // killers, so quiet killer moves don't get scrambled by a SEE comparison
  // that doesn't apply to them.
  if (mTypes != MType::QUIET)
    std::sort(movesArray.begin() + prevS, movesArray.begin() + start, seeComparator);

  // On the CAPTURES stage the captures are now sorted by SEE, so the losing
  // ones (SEE < 0) are at the back. Move `start` back past them. They stay in
  // the array and are searched by the QUIET stage, which plays everything
  // left, so they come after killers, PV moves and checks.
  if (mTypes == MType::CAPTURES)
  {
    while (start > prevS and seeScore(pos, movesArray[start - 1]) < 0)
      --start;
  }

  if constexpr (USE_KILLERS)
  {
    if (hasFlag(mTypes, MType::KILLER))
    {
      for (size_t i = start; i < movesArray.size(); i++) {
        if (killerMoves[ply].contains(filter(movesArray[i])))
          std::swap(movesArray[i], movesArray[start++]);
      }
    }
  }

  // Sort the QUIET stage by history. This comes after the killers so they stay
  // in front. The earlier stages are sorted by SEE, and history only applies
  // to quiet moves.
  //
  // useHistory is false when the caller knows quiet futility will stop the
  // stage at its first move, so the sort would be wasted.
  if constexpr (USE_HISTORY)
  {
    if (hasFlag(mTypes, MType::QUIET) and useHistory)
      sortByHistory(pos.color, movesArray, start);
  }

  return (hasFlag(mTypes, MType::QUIET)) ? movesArray.size() : start;
}

// Capture ordering for quiescence. Every move here is a capture, so only the
// SEE sort from orderMoves() is needed. SEE is computed once per move into an
// array, instead of about 2n*log(n) times inside a comparator.
//
// Returns how many moves from the front to search: every capture with
// SEE >= 0, and at least `floor` moves, so a node where every capture loses
// material still tries its best one.
size_t
orderCaptures(const ChessBoard& pos, MoveArray& movesArray, size_t floor)
{
  const size_t n = movesArray.size();
  array<Score, MAX_MOVES> scores;

  for (size_t i = 0; i < n; i++)
    scores[i] = seeScore(pos, movesArray[i]);

  // Insertion sort, highest first, moving scores[] along with the moves.
  // Capture lists are short (rarely more than a dozen), and at that size this
  // is faster than std::sort.
  for (size_t i = 1; i < n; i++)
  {
    const Move  move  = movesArray[i];
    const Score score = scores[i];
    size_t j = i;

    while (j > 0 and scores[j - 1] < score)
    {
      movesArray[j] = movesArray[j - 1];
      scores[j]     = scores[j - 1];
      --j;
    }

    movesArray[j] = move;
    scores[j]     = score;
  }

  size_t winning = 0;
  while (winning < n and scores[winning] >= 0)
    ++winning;

  return std::max(winning, std::min(floor, n));
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
  Score initialValue =
    (is_type<MType::CAPTURES>(move) and fpt == NONE) ? pieceValues[PAWN] : pieceValues[fpt];

  Bitboard removedPieces = 1ULL << ip;
  PieceType pieceOnSquare = type_of(pos.pieceOnSquare(ip));

  // A promotion turns the pawn into the new piece before the opponent can
  // recapture. Add that gain, and pass see() the promoted piece, not the pawn.
  // Otherwise every promotion would score the same (0 undefended, -100
  // defended), and the PROMOTION stage couldn't tell a queen from a bishop.
  if (is_type<MType::PROMOTION>(move))
  {
    pieceOnSquare = PieceType(((move >> 18) & 3) + 2);
    initialValue += pieceValues[pieceOnSquare] - pieceValues[PAWN];
  }

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
