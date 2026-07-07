
#include "endgame_probe.h"
#include "endgame_utils.h"
#include "attacks.h"

using plt::passedPawnMasks;

template <Endgames e>
inline bool
Endgame(const ChessBoard& pos, Score& dirScore) = delete;

template <>
inline bool
Endgame<Endgames::KPK>(const ChessBoard& pos, Score& dirScore)
{
  const Color side = pos.count<WHITE, PAWN>() ? WHITE : BLACK;
  const Color emySide = ~side;

  const Bitboard    pawn = pos.getPiece(side   , PAWN);
  const Bitboard  myKing = pos.getPiece(side   , KING);
  const Bitboard emyKing = pos.getPiece(emySide, KING);

  const int side2move = pos.color;
  const int incFactor = 2 * side - 1;
  const int sideAdvantage = int(side2move == side);

  const Square    pawnSq = squareNo(pawn   );
  const Square  myKingSq = squareNo(myKing );
  const Square emyKingSq = squareNo(emyKing);

  const int    pawnR = pawnSq    >> 3;
  const int  myKingR = myKingSq  >> 3;
  const int emyKingR = emyKingSq >> 3;

  const int    pawnF = pawnSq    &  7;
  const int  myKingF = myKingSq  &  7;
  const int emyKingF = emyKingSq &  7;
  const int kingFileDiff = myKingF - emyKingF;

  const int pawnOnRank2    =    pawn & relativeRank[side][2] ? 1 : 0;
  const int myKingOnRank8  =  myKing & relativeRank[side][8] ? 1 : 0;
  const int emyKingOnRank8 = emyKing & relativeRank[side][8] ? 1 : 0;
  const int emyKingOnDiag  = int(
    ((pawnR - pawnF) == (emyKingR - emyKingF)) or
    ((pawnR + pawnF) == (emyKingR + emyKingF))
  );

  if (!sideAdvantage and
     (pawn & FileAH) and
     (myKing & (passedPawnMasks[side][pawnSq] & plt::lineMasks[pawnSq])) and
     (abs(kingFileDiff) == 2 or abs(kingFileDiff) == 3) and
     (side == WHITE
      ? emyKingR >= myKingR - 1 - myKingOnRank8
      : emyKingR <= myKingR + 1 + myKingOnRank8)
  ) return true;

  const int ruleOfSquareIndex = getRuleOfSquareIndex(pos, side, pawnSq);

  if (emyKing & ~plt::ruleOfSquares[side][ruleOfSquareIndex]) {
    dirScore = 0  /* directionalScore calculation */;
    return false;
  }

  if (emyKing & plt::ruleOfSquares[side][ruleOfSquareIndex]) {
    int dist = std::max(abs(pawnR - emyKingR), abs(pawnF - emyKingF));

    if ((side == WHITE
         ? (pawnR - dist - sideAdvantage - emyKingOnRank8 >= myKingR)
         : (pawnR + dist + sideAdvantage + emyKingOnRank8 <= myKingR)) and
        (myKingR != emyKingR)
    ) return true;

    const int    sideAdvOffset = sideAdvantage  * pawnOnRank2 * !emyKingOnDiag;
    const int nonSideAdvOffset = !sideAdvantage * pawnOnRank2 * !emyKingOnDiag * 2;

    if (side == WHITE
        ? (myKingR >= pawnR + incFactor + dist + sideAdvantage + emyKingOnDiag + sideAdvOffset + nonSideAdvOffset)
        : (myKingR <= pawnR + incFactor - dist - sideAdvantage - emyKingOnDiag - sideAdvOffset - nonSideAdvOffset)
    ) return true;

    if ((pawnF - dist - 1 - sideAdvantage - pawnOnRank2 >= myKingF) or
        (pawnF + dist + 1 + sideAdvantage + pawnOnRank2 <= myKingF)
    ) return true;
  }

  if ((myKingR == pawnR + 2 * incFactor) and (myKingF == pawnF))
  {
    dirScore = 0  /* directionalScore calculation */;
    return false;
  }

  if ((pawn & FileAH) and (emyKing & passedPawnMasks[side][pawnSq]))
    return true;

  if (sideAdvantage and
      (side == WHITE
        ? (myKingR == pawnR + 1) and (emyKingR == myKingR + 2)
        : (myKingR == pawnR - 1) and (emyKingR == myKingR - 2)) and
      (myKingF == emyKingF) and
     !emyKingOnRank8 and
     !pawnOnRank2
  ) return true;       // 102

  if (!sideAdvantage and
      (pawn & rank2to6[side]) and
      (side == WHITE ? myKingR <= pawnR - 1 : myKingR >= pawnR + 1) and
      (passedPawnMasks[side][pawnSq] & emyKing)
  ) return true;

  if (sideAdvantage and
     (pawn & rank3to5[side]) and
     (side == WHITE ? myKingR <= pawnR - 1 : myKingR >= pawnR + 1) and
     (passedPawnMasks[side][pawnSq] & emyKing)
  ) return true;

  // Rook-pawn corner draw (oracle-mined with bucket-probing).
  // With a rook pawn, if the defending king is closer to the queening corner than
  // the attacking king by a safe margin, the attacker cannot evict it from the
  // corner -- drawn. The margin is one square tighter when the pawn side is to
  // move, since it gains a tempo (sideAdvantage == 1 => needs mdq - edq >= 2).
  if (pawn & FileAH)
  {
    const Square queenSq = static_cast<Square>((side == WHITE ? 56 : 0) + pawnF);
    const int edq = chebyshevDistance(emyKingSq, queenSq);   // defender -> corner
    const int mdq = chebyshevDistance(myKingSq,  queenSq);   // attacker -> corner
    if (mdq - edq >= 1 + sideAdvantage)
      return true;
  }

  return false;
}

EndgameProbe
probeEndgame(const ChessBoard& pos)
{
  EndgameProbe egp{};

  const int pieceCount = pos.count<ALL>();

  if ((pieceCount == 0) or (pieceCount > 2))
    return egp;

  if (pieceCount == 1)
  {
    if (isEndgame<Endgames::KPK>(pos))
    {
      egp.signatureHit = true;
      egp.positionHit  = Endgame<Endgames::KPK>(pos, egp.directionalScore);
      return egp;
    }

    if (isEndgame<Endgames::KBK>(pos) or isEndgame<Endgames::KNK>(pos))
    {
      egp.signatureHit = true;
      egp.positionHit  = true;
    }

    return egp;
  }


  return egp;
}


Score
calcPromotionScore(const ChessBoard& pos, Color winningSide)
{
  Bitboard pawns = pos.getPiece(winningSide, PAWN);
  Score score = 0;
  while (pawns)
  {
    const Square pawnSq = nextSquare(pawns);
    const int pawnR = pawnSq >> 3;
    score += winningSide == WHITE ? (40 * pawnR) : -(40 * (7 - pawnR));
  }
  return score;
}

Score
calcDirectionScore(const ChessBoard& pos, Color winningSide)
{
  const Score pawnPromotionScore = calcPromotionScore(pos, winningSide);

  // calcPromotionScore is white-relative (positive => good for white); folding in
  // the winner's sign strips that back to a positive winner-relative magnitude.
  return pawnPromotionScore * (2 * winningSide - 1);
}


