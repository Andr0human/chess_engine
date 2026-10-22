
#include "endgame_utils.h"


namespace eg
{
  bool
  kingBetweenQueens(const Square kingSq, const Bitboard queen1, const Bitboard queen2)
  {
    auto between = [&](Bitboard pos, Bitboard neg) {
      return ((pos & queen1) and (neg & queen2)) or
             ((pos & queen2) and (neg & queen1));
    };

    return between(plt::upMasks[kingSq],      plt::downMasks[kingSq])      or
           between(plt::leftMasks[kingSq],    plt::rightMasks[kingSq])     or
           between(plt::upLeftMasks[kingSq],  plt::downRightMasks[kingSq]) or
           between(plt::upRightMasks[kingSq], plt::downLeftMasks[kingSq]);
  }

  int
  getRuleOfSquareIndex(const EgBoard& pos, Color side, Square pawnSq)
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
}
