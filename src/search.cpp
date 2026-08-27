
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

// Order the residual QUIET band by history score, descending.
//
// Scored once into a parallel array rather than inside a comparator (the
// orderCaptures pattern) — the band averages ~26 moves, so a comparator would
// redo the same three-level lookup ~2n*log(n) times. The insertion sort is
// stable under `<`, which matters: v1 never applies malus, so every move that
// has not yet cut off sits at 0, and those keep their emission order instead of
// being shuffled arbitrarily among themselves.
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

  // Bad-capture demotion. On the dedicated CAPTURES
  // stage the band is now SEE-sorted descending, so SEE<0 captures sit at
  // the back. Pull `start` left past them — they stay in the array and get
  // picked up by the QUIET-tail stage (which plays everything remaining),
  // i.e. *after* killers/PV/checks. Search effort no longer gets spent on
  // e.g. QxP-defended-by-pawn before any quiet move is tried.
  if (mTypes == MType::CAPTURES)
  {
    while (start > prevS and seeScore(pos, movesArray[start - 1]) < 0)
      --start;
  }

  if (hasFlag(mTypes, MType::KILLER))
  {
    for (size_t i = start; i < movesArray.size(); i++) {
      if (killerMoves[ply].search(movesArray[i]))
        std::swap(movesArray[i], movesArray[start++]);
    }
  }

  // History ordering of the residual QUIET band. Placed after the killer
  // partition so killers keep their slot ahead of history, and only on the
  // QUIET stage — the earlier stages are already ordered by SEE and would be
  // scrambled by a key that only means anything for quiets.
  //
  // `useHistory` is the call site's veto: it knows (and orderMoves doesn't) when
  // the stage is about to break on move 0 under quiet futility, where the sort
  // is pure waste — measured at ~29% of shallow quiet stages.
  if constexpr (USE_HISTORY)
  {
    if (hasFlag(mTypes, MType::QUIET) and useHistory)
      sortByHistory(pos.color, movesArray, start);
  }

  return (hasFlag(mTypes, MType::QUIET)) ? movesArray.size() : start;
}

// Capture ordering for quiescence search. Unlike orderMoves(), every move here
// is already known to be a capture, so the prioritize/killer/PV staging is dead
// weight -- all that's left is the SEE sort. Scoring once into a parallel array
// instead of inside a comparator cuts seeScore() calls from ~2n*log(n) to n.
//
// Returns the number of leading moves worth searching: every SEE >= 0 capture,
// plus `floor` moves regardless, so a node whose captures all lose material
// still searches its best try rather than collapsing to stand-pat.
size_t
orderCaptures(const ChessBoard& pos, MoveArray& movesArray, size_t floor)
{
  const size_t n = movesArray.size();
  array<Score, MAX_MOVES> scores;

  for (size_t i = 0; i < n; i++)
    scores[i] = seeScore(pos, movesArray[i]);

  // Insertion sort, descending, moving scores[] in lockstep. Capture lists are
  // short (rarely past a dozen), where this beats std::sort's setup cost.
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

  // A promotion swaps the pawn for the promoted piece before the opponent can
  // recapture. Credit that material gain, and hand see() the piece that
  // actually lands on `fp` rather than the pawn that left `ip` -- otherwise
  // every flavor of a promotion scores identically (0 undefended, -100
  // defended), so the PROMOTION ordering stage cannot tell a queen from a
  // bishop and capture-promotions sort by victim alone.
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
