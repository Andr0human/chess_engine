
#ifndef ENDGAME_UTILS_H
#define ENDGAME_UTILS_H

#include "endgame.h"

// Helpers shared by the recognizers in endgame.cpp.
namespace eg
{
  // Ranks indexed relative to each side's home rank. Index 0 is unused.
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

  // The next two are defined here so the recognizers inline them. From
  // endgame_utils.cpp, LTO leaves them as calls.

  // How many squares `s` is from the nearest edge: 0 on the edge, 3 in the centre.
  inline int
  edgeDistance(const Square s)
  {
    const int r = s >> 3, f = s & 7;
    return std::min(std::min(r, 7 - r), std::min(f, 7 - f));
  }

  // A king in a corner with its own piece next to it. The piece blocks one of
  // the king's flight squares, so a minor apiece (KBKB, KNKN, KBKN) can be lost
  // here. Every won position in those three signatures has this shape.
  inline bool
  cornerBlockedByOwnPiece(const Bitboard king, const Bitboard piece)
  { return (king & CornerSquares) and (plt::chebyshevDistance(squareNo(king), squareNo(piece)) == 1); }

  bool
  kingBetweenQueens(Square kingSq, Bitboard queen1, Bitboard queen2);

  int
  getRuleOfSquareIndex(const EgBoard& pos, Color side, Square pawnSq);

  // KRB vs KR and KRN vs KR. The defender (the lone rook's side) is to move and
  // not in check; the attacker has a rook and a `Minor`.

  // A back-rank threat: the kings are two apart, the attacking king is on the
  // edge with the defending king two lines in front of it, and the defending
  // rook reaches one of the attacking king's lines.
  bool
  backRankThreat(const EgBoard& pos);

  // The defending rook mates in one: a rook move checks the attacking king, the
  // king has no square to go to, and the rook or minor can neither take the
  // checking rook nor block the check. Only a king on the edge can be mated by
  // a rook.
  template <PieceType Minor>
  bool
  rookMatesInOne(const EgBoard& pos);
}

#endif
