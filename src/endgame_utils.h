
#ifndef ENDGAME_UTILS_H
#define ENDGAME_UTILS_H

#include "bitboard.h"
#include "lookup_table.h"

// ============================================================================
//  Endgame utilities -- shared plumbing for the theoretical-draw recognizers
//  (endgame.cpp) and the endgame probe (endgame_probe.cpp).
//
//  Two groups live here:
//    1. Geometry helpers + color-relative rank tables used inside recognizer
//       bodies.
//    2. isEndgame<> -- material-signature detection. The verdict logic
//       (Endgame<> / probeEndgame) differs per consumer, but "which endgame is
//       this?" is identical, so it is shared.
// ============================================================================


// --- Geometry helpers -------------------------------------------------------

inline int
chebyshevDistance(Square s1, Square s2)
{
  int rank1 = s1 >> 3, file1 = s1 & 7;
  int rank2 = s2 >> 3, file2 = s2 & 7;
  return std::max(abs(rank1 - rank2), abs(file1 - file2));
}

// Is the king on a ray strictly between piece1 and piece2 (one on each side)?
// lineCheck scans the rank/file rays, diagCheck the diagonals -- pick per attacker:
// rook -> <true,false>, bishop -> <false,true>, queen -> <true,true>. Unused
// branches fold away at compile time.
template <bool lineCheck, bool diagCheck>
inline bool
kingBetweenPieces(const Square kingSq, const Bitboard piece1, const Bitboard piece2)
{
  auto between = [&](Bitboard pos, Bitboard neg) {
    return ((pos & piece1) and (neg & piece2)) or
           ((pos & piece2) and (neg & piece1));
  };

  bool result = false;

  if constexpr (lineCheck)
    result = result
          or between(plt::upMasks[kingSq],   plt::downMasks[kingSq])
          or between(plt::leftMasks[kingSq], plt::rightMasks[kingSq]);

  if constexpr (diagCheck)
    result = result
          or between(plt::upLeftMasks[kingSq],  plt::downRightMasks[kingSq])
          or between(plt::upRightMasks[kingSq], plt::downLeftMasks[kingSq]);

  return result;
}

inline bool
kingBetweenQueens(const Square kingSq, const Bitboard queen1, const Bitboard queen2)
{ return kingBetweenPieces<true, true>(kingSq, queen1, queen2); }


// --- Color-relative rank tables ---------------------------------------------

// Color-relative ranks: relativeRank[color][r] is the r-th rank (1-8) counting
// from color's own back rank, so the BLACK row mirrors to absolute rank 9-r.
// Ranks are 1-based; index 0 is a NoSquares placeholder so call sites read naturally.
inline constexpr Bitboard relativeRank[COLOR_NB][9] = {
  { NoSquares, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1 },  // BLACK
  { NoSquares, Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8 },  // WHITE
};

inline constexpr Bitboard rank2to6[COLOR_NB] = {
  AllSquares ^ (Rank18 | Rank2),
  AllSquares ^ (Rank18 | Rank7)
};

inline constexpr Bitboard rank3to5[COLOR_NB] = {
  Rank4 | Rank5 | Rank6,
  Rank3 | Rank4 | Rank5
};

inline constexpr Bitboard rank4to7[COLOR_NB] = {
  Rank4 | Rank5 | Rank6 | Rank7,
  Rank2 | Rank3 | Rank4 | Rank5
};


// --- Rule-of-the-square index -----------------------------------------------

inline int
getRuleOfSquareIndex(const ChessBoard& pos, Color side, Square pawnSq)
{
  const int sideAdvantage = int(side == pos.color);
  const int incFactor     = 2 * int(side) - 1;

  const Bitboard myKing = pos.getPiece(side, KING);
  const Bitboard   pawn = pos.getPiece(side, PAWN);

  int ruleOfSquareIndex = pawnSq;

  if (!sideAdvantage)
    ruleOfSquareIndex -= 8 * incFactor;

  if ((pawn & relativeRank[side][2]) and (myKing & ~plt::pawnMasks[side][pawnSq]))
    ruleOfSquareIndex += 8 * incFactor;

  return ruleOfSquareIndex;
}


// --- Material-signature detection -------------------------------------------
//
// isEndgame<E>(pos) answers "does pos have exactly material signature E?" with
// no regard to who is winning. Shared verbatim by every consumer; extend by
// adding a specialization here (and the enum entry in types.h) for new
// signatures (KRRK, KRNK, ...).

template <Endgames e>
inline bool
isEndgame(const ChessBoard& pos) = delete;

template <>
inline bool
isEndgame<Endgames::KPK>(const ChessBoard& pos)
{ return pos.count<ALL>() == 1 and pos.count<PAWN>() == 1; }

template <>
inline bool
isEndgame<Endgames::KNK>(const ChessBoard& pos)
{ return pos.count<ALL>() == 1 and pos.count<KNIGHT>() == 1; }

template <>
inline bool
isEndgame<Endgames::KBK>(const ChessBoard& pos)
{ return pos.count<ALL>() == 1 and pos.count<BISHOP>() == 1; }

template <>
inline bool
isEndgame<Endgames::KPBK>(const ChessBoard& pos)
{
  return pos.count<ALL   >() == 2
     and pos.count<PAWN  >() == 1
     and pos.count<BISHOP>() == 1;
}

template <>
inline bool
isEndgame<Endgames::KPQK>(const ChessBoard& pos)
{
  return pos.count<ALL   >() == 2
     and pos.count<PAWN  >() == 1
     and pos.count<QUEEN>() == 1;
}

template <>
inline bool
isEndgame<Endgames::KBBK>(const ChessBoard& pos)
{ return pos.count<ALL>() == 2 and pos.count<BISHOP>() == 2; }

template <>
inline bool
isEndgame<Endgames::KNNK>(const ChessBoard& pos)
{ return pos.count<ALL>() == 2 and pos.count<KNIGHT>() == 2; }

template <>
inline bool
isEndgame<Endgames::KBNK>(const ChessBoard& pos)
{
  return pos.count<ALL   >() == 2
     and pos.count<BISHOP>() == 1
     and pos.count<KNIGHT>() == 1;
}

template<>
inline bool
isEndgame<Endgames::KRBK>(const ChessBoard& pos)
{
  return pos.count<ALL   >() == 2
     and pos.count<ROOK  >() == 1
     and pos.count<BISHOP>() == 1;
}

template <>
inline bool
isEndgame<Endgames::KPNK>(const ChessBoard& pos)
{
  return pos.count<ALL   >() == 2
     and pos.count<PAWN  >() == 1
     and pos.count<KNIGHT>() == 1;
}


#endif
