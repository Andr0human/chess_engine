
#include "endgame_utils.h"
#include "attacks.h"


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

  bool
  backRankThreat(const EgBoard& pos)
  {
    const Color def = pos.color, atk = ~def;

    const Bitboard akB = pos.getPiece(atk, KING);
    const Bitboard dkB = pos.getPiece(def, KING);
    const Square ak = squareNo(akB), dk = squareNo(dkB);
    if (plt::chebyshevDistance(ak, dk) != 2)
      return false;

    // The attacking rook and minor block the king's lines.
    const Bitboard aMen  = pos.getPiece(atk, ALL) ^ akB;
    const Bitboard drAtt = attackSquares<ROOK>(squareNo(pos.getPiece(def, ROOK)), pos.all());

    return (((akB & Rank18) and std::abs((ak >> 3) - (dk >> 3)) == 2) or
            ((akB & FileAH) and std::abs((ak  & 7) - (dk  & 7)) == 2))
        and (drAtt & ~dkB & attackSquares<ROOK>(ak, aMen));
  }

  template <PieceType Minor>
  bool
  rookMatesInOne(const EgBoard& pos)
  {
    const Color def = pos.color, atk = ~def;

    const Bitboard akB = pos.getPiece(atk, KING);
    const Square   ak  = squareNo(akB);
    if (edgeDistance(ak) != 0)
      return false;

    const Bitboard arB = pos.getPiece(atk, ROOK );
    const Bitboard amB = pos.getPiece(atk, Minor);
    const Bitboard dkB = pos.getPiece(def, KING );
    const Bitboard drB = pos.getPiece(def, ROOK );

    const Square ar = squareNo(arB), am = squareNo(amB);
    const Square dk = squareNo(dkB), dr = squareNo(drB);
    const Bitboard occ    = pos.all();
    const Bitboard drLift = occ ^ drB;

    const Bitboard akAtt = attackSquares<KING>(ak, 0);
    const Bitboard dkAtt = attackSquares<KING>(dk, 0);
    const Bitboard drAtt = attackSquares<ROOK>(dr, occ);

    for (Bitboard t = drAtt & attackSquares<ROOK>(ak, occ) & ~dkB; t != 0; t &= t - 1)
    {
      const Bitboard sB     = t & -t;
      const Square   s      = squareNo(sB);
      const Bitboard o      = drLift | sB;
      const Bitboard arLeft = arB & ~sB;
      const Bitboard amLeft = amB & ~sB;
      const Bitboard arO    = attackSquares<ROOK >(ar, o);
      const Bitboard amO    = attackSquares<Minor>(am, o);

      // A rook pinned to its king by the attacking rook or bishop can't move off
      // the line. (A knight hitting the king would be a check.)
      if ((arLeft and (arO & dkB)) or (amLeft and (amO & dkB)))
        continue;

      // Each king square is guarded by the rook (seen through the king,
      // 8/8/8/1r6/8/8/8/K1kN1R2 b) or the defending king, or holds the king's
      // own rook or minor (8/8/8/1r6/8/8/R7/KNk5 b). The king takes the checking
      // rook unless the defending king guards it.
      if (akAtt & ~dkAtt & ~attackSquares<ROOK>(s, o ^ akB) & ~(arLeft | amLeft))
        continue;

      // The rook or minor takes the checking rook or blocks the check.
      const Bitboard stop = sB | (attackSquares<ROOK>(s, o) & attackSquares<ROOK>(ak, o));
      if ((arLeft and (arO & stop)) or (amLeft and (amO & stop)))
        continue;

      return true;
    }

    return false;
  }

  template bool rookMatesInOne<KNIGHT>(const EgBoard& pos);
  template bool rookMatesInOne<BISHOP>(const EgBoard& pos);
}
