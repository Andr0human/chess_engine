
#include "endgame.h"
#include "endgame_utils.h"
#include "attacks.h"

using plt::passedPawnMasks;
using plt::ruleOfSquares;

// Distance helpers from lookup_table.h.
using plt::chebyshevDistance;
using plt::manhattanDistance;

// Helpers shared by the recognizers.
using namespace eg;

// The dispatcher, declared here so a recognizer can ask about the position after
// a capture (`isTheoreticalDraw(pos.play(from, to))`).
static bool
isTheoreticalDraw(const EgBoard& pos);


// TODO: KRRK

template <Endgames e>
inline bool
isEndgame(const EgBoard& pos) = delete;

template <>
inline bool
isEndgame<Endgames::KPK>(const EgBoard& pos)
{ return pos.count<ALL>() == 1 and pos.count<PAWN>() == 1; }

template <>
inline bool
isEndgame<Endgames::KNK>(const EgBoard& pos)
{ return pos.count<ALL>() == 1 and pos.count<KNIGHT>() == 1; }

template <>
inline bool
isEndgame<Endgames::KBK>(const EgBoard& pos)
{ return pos.count<ALL>() == 1 and pos.count<BISHOP>() == 1; }

template <>
inline bool
isEndgame<Endgames::KPBK>(const EgBoard& pos)
{
  return pos.count<ALL   >() == 2
     and pos.count<PAWN  >() == 1
     and pos.count<BISHOP>() == 1;
}

template <>
inline bool
isEndgame<Endgames::KPQK>(const EgBoard& pos)
{
  return pos.count<ALL   >() == 2
     and pos.count<PAWN  >() == 1
     and pos.count<QUEEN>() == 1;
}

template <>
inline bool
isEndgame<Endgames::KBBK>(const EgBoard& pos)
{ return pos.count<ALL>() == 2 and pos.count<BISHOP>() == 2; }

template <>
inline bool
isEndgame<Endgames::KNNK>(const EgBoard& pos)
{ return pos.count<ALL>() == 2 and pos.count<KNIGHT>() == 2; }

template <>
inline bool
isEndgame<Endgames::KBNK>(const EgBoard& pos)
{
  return pos.count<ALL   >() == 2
     and pos.count<BISHOP>() == 1
     and pos.count<KNIGHT>() == 1;
}

template<>
inline bool
isEndgame<Endgames::KRBK>(const EgBoard& pos)
{
  return pos.count<ALL   >() == 2
     and pos.count<ROOK  >() == 1
     and pos.count<BISHOP>() == 1;
}

template <>
inline bool
isEndgame<Endgames::KRNK>(const EgBoard& pos)
{
  return pos.count<ALL   >() == 2
     and pos.count<ROOK  >() == 1
     and pos.count<KNIGHT>() == 1;
}

template <>
inline bool
isEndgame<Endgames::KRKR>(const EgBoard& pos)
{ return pos.count<ALL>() == 2 and pos.count<ROOK>() == 2; }

template <>
inline bool
isEndgame<Endgames::KPNK>(const EgBoard& pos)
{
  return pos.count<ALL   >() == 2
     and pos.count<PAWN  >() == 1
     and pos.count<KNIGHT>() == 1;
}

template <>
inline bool
isEndgame<Endgames::KPRK>(const EgBoard& pos)
{
  return pos.count<ALL >() == 2
     and pos.count<PAWN>() == 1
     and pos.count<ROOK>() == 1;
}

template <>
inline bool
isEndgame<Endgames::KBNKN>(const EgBoard& pos)
{
  return pos.count<ALL   >() == 3
     and pos.count<BISHOP>() == 1
     and pos.count<KNIGHT>() == 2;
}

template <>
inline bool
isEndgame<Endgames::KRBKR>(const EgBoard& pos)
{
  return pos.count<ALL   >() == 3
     and pos.count<ROOK  >() == 2
     and pos.count<BISHOP>() == 1;
}

template <>
inline bool
isEndgame<Endgames::KRNKR>(const EgBoard& pos)
{
  return pos.count<ALL   >() == 3
     and pos.count<ROOK  >() == 2
     and pos.count<KNIGHT>() == 1;
}

template <Endgames e>
inline bool
Endgame(const EgBoard& pos) = delete;

template <>
inline bool
Endgame<Endgames::KPK>(const EgBoard& pos)
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
     (myKing & (plt::passedPawnMasks[side][pawnSq] & plt::lineMasks[pawnSq])) and
     (abs(kingFileDiff) == 2 or abs(kingFileDiff) == 3) and
     (side == WHITE
      ? emyKingR >= myKingR - 1 - myKingOnRank8
      : emyKingR <= myKingR + 1 + myKingOnRank8)
  ) return true;

  const int ruleOfSquareIndex = getRuleOfSquareIndex(pos, side, pawnSq);

  if (emyKing & ~plt::ruleOfSquares[side][ruleOfSquareIndex])
    return false;

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

  if ((myKingR == pawnR + 2 * incFactor) and (myKingF == pawnF)
  ) return false;

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

  // Rook-pawn corner draw when the defending king can reach the queening corner.
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

template <>
inline bool
Endgame<Endgames::KPBK>(const EgBoard& pos)
{
  // Evaluate from the bishop side.
  const Color side2move = pos.color;
  const Color side = pos.count<WHITE, BISHOP>() ? WHITE : BLACK;
  const Color emySide = ~side;

  const Bitboard occupied = pos.all();
  const Bitboard bishop   = pos.getPiece(side,  BISHOP);
  const Bitboard myKing   = pos.getPiece(side,    KING);
  const Bitboard emyKing  = pos.getPiece(emySide, KING);

  const Square   bishopSq = squareNo(bishop);
  const Square     kingSq = squareNo(myKing);
  const Square  emyKingSq = squareNo(emyKing);

  Bitboard kingCapMask    = attackSquares<KING>(kingSq   , 0);
  Bitboard emyKingCapMask = attackSquares<KING>(emyKingSq, 0);
  Bitboard bishopCapMask  = attackSquares<BISHOP>(bishopSq, myKing);

  if (pos.count<WHITE, ALL>() == 1)
  {
    // Pawn and bishop are of different side
    const Bitboard pawn = pos.getPiece(emySide, PAWN);
    const Square pawnSq = squareNo(pawn);
    const int     pawnR = pawnSq >> 3;

    Bitboard extEmyKingCapMask = 0;

    // If the bishop can be taken immediately, defer to search.
    if ((side2move == emySide) and
       ((bishop & plt::pawnCaptureMasks[emySide][pawnSq]) or
        (bishop & emyKingCapMask & ~kingCapMask)))
      return false;

    {
      const Bitboard emyKingLegalMask = emyKingCapMask & ~(occupied | bishopCapMask | kingCapMask);
      if ((emyKingLegalMask == 0) and
          (side2move == side) and
          (attackSquares<BISHOP>(emyKingSq, occupied) & bishopCapMask)
      ) return false;

      if ((side2move == side) and
          (emyKing & FileAH) and
          (emyKing & relativeRank[emySide][1]) and
          (emyKing & attackSquares<BISHOP>(bishopSq, 0))
      ) return false;
    }

    {
      const Bitboard kingLegalMask = kingCapMask & ~(occupied | emyKingCapMask);
      if (~kingLegalMask and (myKing & plt::pawnCaptureMasks[emySide][pawnSq + 8 * (2 * emySide - 1)]))
        return false;
    }

    if (side2move == emySide)
    {
      Bitboard temp = emyKingCapMask;
      while (temp)
      {
        const Square sq = nextSquare(temp);
        extEmyKingCapMask |= attackSquares<KING>(sq, 0);
      }
    }

    Bitboard pawnMask = passedPawnMasks[emySide][pawnSq] & plt::lineMasks[pawnSq];

    if (myKing & pawnMask)
      return true;

    if ((bishop & pawnMask) and
       ((bishopCapMask & ~emyKing & ~myKing) & ~emyKingCapMask)
    ) return true;

    if ((bishopCapMask & pawnMask & ~emyKingCapMask) and
        ((side2move == side) or ((side2move == emySide) and (bishop & ~extEmyKingCapMask)))
    ) return true;

    if ((plt::pawnCaptureMasks[emySide][pawnSq] & myKing) and
        (emySide == WHITE ? pawnR > 4 : pawnR < 3)
    ) return false;

    if (side2move == emySide) {
      pawnMask ^= pawnMask & plt::pawnMasks[emySide][pawnSq];

      if ((plt::pawnCaptureMasks[emySide][pawnSq + 8 * (2 * emySide - 1)] & myKing) and
          (emyKingCapMask & (1ULL << (pawnSq + 8 * (2 * emySide - 1))))
      ) pawnMask ^= pawnMask & emyKingCapMask;
    }
    
    const Bitboard dangerMask = emyKingCapMask | myKing | ((side2move == emySide) * extEmyKingCapMask);

    while (pawnMask)
    {
      const Square sq = nextSquare(pawnMask);
      const Bitboard bishopSquares = bishopCapMask & attackSquares<BISHOP>(sq, myKing);

      const Bitboard intersection = bishopSquares & ~dangerMask;

      if (intersection & ~relativeRank[emySide][8]) {
        return true;
      }
    }

    return false;
  }

  const Bitboard pawn = pos.getPiece(side, PAWN);
  const Square pawnSq = squareNo(pawn);

  const int pawnR    = pawnSq    >> 3;
  const int kingR    = kingSq    >> 3;
  const int emyKingR = emyKingSq >> 3;

  Bitboard pawnMask = passedPawnMasks[side][pawnSq] & plt::lineMasks[pawnSq];
  Bitboard cornerMask = pawnMask & Rank18;
  const int increment = int(side2move == side);

  // Wrong-bishop rook-pawn draw.
  if ((pawn & FileAH) and
      (emyKing & passedPawnMasks[side][pawnSq]) and
      ((side == WHITE ? emyKingR > kingR + increment : emyKingR < kingR - increment)
    or (emyKing & (relativeRank[side][7] | relativeRank[side][8]))) and
      (((cornerMask & WhiteSquares) and !(bishop & WhiteSquares))
    or ((cornerMask & BlackSquares) and !(bishop & BlackSquares)))
  ) return true;

  // Not a theoretical draw if, pawn is protected by bishop or king
  // or is closer to promo square than enemy king
  // or the pawn can reach a square above protected by its king
  if ((pawn & kingCapMask) or
      (pawn & bishopCapMask) or
      ((side == WHITE ? pawnR > emyKingR : pawnR < emyKingR) and !(plt::pawnMasks[side][pawnSq] & bishop)) or
      (!(plt::pawnMasks[side][pawnSq] & occupied) and (attackSquares<KING>(pawnSq + 8 * (2 * side - 1), 0) & myKing))
  ) return false;

  // Pawn attacked by the enemy king without sufficient king support.
  if ((pawn & emyKingCapMask) and !(kingCapMask & attackSquares<KING>(pawnSq, 0) & ~emyKingCapMask)) {
    // If bishop is just above it
    if ((plt::pawnMasks[side][pawnSq] & bishop))
      return true;
    
    // If bishop cannot support it
    if (!(pawn & relativeRank[side][2]) and
       (!(bishopCapMask & plt::pawnMasks[side][pawnSq]) and !(bishopCapMask & attackSquares<BISHOP>(pawnSq, occupied)))
    ) return true;
  }

  return false;
}

template <>
inline bool
Endgame<Endgames::KPQK>(const EgBoard& pos)
{
  // Look from the pawn side (the side that does not hold the queen)
  const Color side2move = pos.color;
  const Color side = pos.count<WHITE, QUEEN>() ? BLACK : WHITE;
  const Color emySide = ~side;

  const int sideAdvantage = int(side2move == side);

  const Bitboard occupied = pos.all();
  const Bitboard myKing   = pos.getPiece(side   ,  KING);
  const Bitboard emyKing  = pos.getPiece(emySide,  KING);
  const Bitboard queen    = pos.getPiece(emySide, QUEEN);

  const Square    kingSq = squareNo(myKing);
  const Square emyKingSq = squareNo(emyKing);
  const Square   queenSq = squareNo(queen);

  Bitboard kingMask    = attackSquares<KING >(kingSq   , occupied);
  Bitboard emyKingMask = attackSquares<KING >(emyKingSq, 0);
  Bitboard queenMask   = attackSquares<QUEEN>(queenSq, emyKing);

  if (pos.count<WHITE, ALL>() == 1)
  {
    const Bitboard pawn = pos.getPiece(side, PAWN);
    const Square pawnSq = squareNo(pawn);
    Bitboard pawnMask   = passedPawnMasks[side][pawnSq] & plt::lineMasks[pawnSq];

    const int  myKingR = kingSq    >> 3;
    const int  myKingF = kingSq     & 7;
    const int emyKingR = emyKingSq >> 3;
    const int emyKingF = emyKingSq  & 7;
    const int    pawnF = pawnSq     & 7;
    const int    pawnR = pawnSq    >> 3;
    const int   queenF = queenSq    & 7;
    const int   queenR = queenSq   >> 3;
    const Square promoSq = static_cast<Square>((side == WHITE ? 56 : 0) + pawnF);

    const bool pawnOnRank7 = !!(pawn & relativeRank[side][7]);
    const bool kingNotOnPromoSq = !(myKing & (1ULL << promoSq));
    const int  distanceBtwKings = chebyshevDistance(kingSq, emyKingSq);

    const int    dkPromoDist  = chebyshevDistance(kingSq   , promoSq);
    const int    daPromoDist  = chebyshevDistance(emyKingSq, promoSq);
    const int    pawnEdgeFile = std::min(pawnF, 7 - pawnF);

    // If the queen can be taken immediately, defer to search.
    if (queen & (plt::kingMasks[kingSq] | plt::pawnCaptureMasks[side][pawnSq]))
      return false;

    // Rook- and bishop-pawn fortress positions.
    if (pawnOnRank7 and (kingMask & pawn) and
       ((pawnEdgeFile == 0) or (pawnEdgeFile == 2)) and
        (dkPromoDist <= 1) and
       !((pawnEdgeFile == 0) and !sideAdvantage and (dkPromoDist == 0)) and
        (daPromoDist >= 6 - int(sideAdvantage and (pawnEdgeFile == 2)))
    ) return true;

    if (side == WHITE ? pawnR < 5 : pawnR > 2)
      return false;

    if ((emyKing | queen) & pawnMask)
      return false;   // Leaves 1 safe gap with queen included (8/8/8/1p6/8/KQ6/8/k7 b - - 0 1)

    if (sideAdvantage and
       pawnOnRank7 and
       kingNotOnPromoSq and
       (myKing & ~queenMask)
    ) {
      // Condition-1
      if (kingBetweenQueens(emyKingSq, queen, 1ULL << promoSq) and !kingBetweenQueens(kingSq, queen, 1ULL << promoSq)) {
        return (chebyshevDistance(emyKingSq, queenSq) == 1)
           and (distanceBtwKings > 2)
           and (queen & ~(FileAH & Rank18))
           and !(emyKingMask & (1ULL << promoSq));
      }                 // 1391
    }

    if (!sideAdvantage and
       (emyKing & ruleOfSquares[side][pawnSq]) and
       (emyKing & ~(kingMask | plt::pawnCaptureMasks[side][pawnSq]))
    ) return false;

    // if pawn can promote,
    // opp (queen | king) cannot capture,
    // own king is not on promo square
    // Condition 1 pre-check.
    if (sideAdvantage and
       pawnOnRank7 and
       kingNotOnPromoSq and
       !((queenMask | emyKingMask) & (1ULL << promoSq)) and
       !((queenMask | emyKingMask) & myKing)
    ) {
      if ((emyKing & relativeRank[side][8]) and
          (queen   & relativeRank[side][7]) and
          ((pawnF - queenF) * (pawnF - emyKingF) > 0) and
          (abs(pawnF - queenF) > abs(pawnF - emyKingF)) and
          (chebyshevDistance(emyKingSq, queenSq) > 3)
      ) return false;

      if ((emyKingF == pawnF) and
          (abs(queenF - emyKingF) == 1) and
          (abs(pawnR - queenR) > abs(pawnR - emyKingR)) and
          (chebyshevDistance(emyKingSq, queenSq) > 3)
      ) return false;

      // Check for queen lines that can win the promoted queen.
      const bool queenLineDraw =
           (dkPromoDist == 1 and distanceBtwKings > daPromoDist)
        or (dkPromoDist == 1 and daPromoDist >= 4 and distanceBtwKings >= 4)
        or (dkPromoDist == 2 and daPromoDist >= 5 and distanceBtwKings >= 6);

      // Orthogonal queen lines require a stronger position than diagonal lines.
      const bool queenLineDrawOrthogonal =
           queenLineDraw
        or (dkPromoDist == 2 and daPromoDist >= 4);

      if ((myKingF == pawnF) or (abs(myKingF - pawnF) == 1 and (myKing & FileAH)))
      {
        Bitboard mask = side == WHITE ? plt::downMasks[kingSq] : plt::upMasks[kingSq];
        mask &= ~kingMask;
        if (queenMask & mask)
          return queenLineDrawOrthogonal;
      }

      // a1-h8 diagonal.
      if ((myKingF - myKingR) == (pawnF - (promoSq >> 3)))
      {
        Bitboard mask = side == WHITE ? plt::downLeftMasks[kingSq] : plt::upRightMasks[kingSq];
        mask &= ~kingMask;
        if (queenMask & mask)
          return queenLineDraw;
      }

      // a8-h1 diagonal.
      if ((myKingF + myKingR) == (pawnF + (promoSq >> 3)))
      {
        Bitboard mask = side == WHITE ? plt::downRightMasks[kingSq] : plt::upLeftMasks[kingSq];
        mask &= ~kingMask;
        if (queenMask & mask)
          return queenLineDraw;
      }

      if ((myKingR == (promoSq >> 3)))
      {
        Bitboard mask = (pawnF > myKingF) ? plt::leftMasks[kingSq] : plt::rightMasks[kingSq];
        mask &= ~kingMask;
        if (queenMask & mask)
          return queenLineDrawOrthogonal;
      }

      if ((kingMask & ~(queenMask | emyKingMask)) and
          (distanceBtwKings > 2 + int(!!(myKing & EdgeSquares))) and
         !((emyKingMask & queen) and (queen & CornerSquares)) and
         !((queenMask | emyKingMask) & myKing)
      ) return true;
    }

    // Condition 1 pre-check.
    if (sideAdvantage and
        (myKing & (relativeRank[side][8] & FileAH)) and
        pawnOnRank7 and
        (abs(pawnF - myKingF) == 2) and
        (abs(pawnF - emyKingF)   > 2)
    ) return true;      // 2729

    if (pawnOnRank7 and
        sideAdvantage and
       (myKing & (relativeRank[side][7] | relativeRank[side][8])) and
       (chebyshevDistance(pawnSq, emyKingSq) > 2) and
       (((pawn & FileC) and (myKing & (FileA | FileB)))
     or ((pawn & FileF) and (myKing & (FileG | FileH))))
    ) return true;

    // Two files towards the centre from the king. The guard below admits a king
    // on the b-/g-file as well as the rook file, so keying the direction off
    // `myKing & FileA` steps the wrong way there: b8 - 2 leaves the rank
    // entirely (h7) and b1 - 2 is a negative shift. Key it off the board half.
    const int toReachSq = kingSq + (myKingF <= 3 ? 2 : -2);

    if ((pawn & FileAH) and
        (myKing & relativeRank[side][8]) and
        (chebyshevDistance(pawnSq, kingSq) < 2) and
        (chebyshevDistance(pawnSq, emyKingSq) > 4 + !sideAdvantage) and
       !(queenMask & (1ULL << toReachSq))
    ) return true;

    // Pawn on the 7th with the defending king near promotion and the attacking
    // king too far away.
    if (sideAdvantage and pawnOnRank7 and
        (dkPromoDist == 1) and
       !(queenMask & myKing) and
        (manhattanDistance(kingSq, emyKingSq) >= 5)
    ) return true;

    return false;
  }

  return false;
}


template <>
inline bool
Endgame<Endgames::KNNK>(const EgBoard& pos)
{
  // A knight apiece (KNKN): drawn unless a king is cornered by its own knight.
  if (pos.count<WHITE, KNIGHT>() == 1)
  {
    return !(cornerBlockedByOwnPiece(pos.piece<WHITE, KING>(), pos.piece<WHITE, KNIGHT>())
          or cornerBlockedByOwnPiece(pos.piece<BLACK, KING>(), pos.piece<BLACK, KNIGHT>()));
  }

  // Two knights against a bare king can't force mate, so the only wins are
  // positions where the bare king is already caught. All of them have the
  // knight side to move, the bare king on an edge, the kings two squares
  // apart, and at most one flight square. Anything else is a draw.
  const Color atkSide = pos.count<WHITE, KNIGHT>() ? WHITE : BLACK;

  if (pos.color != atkSide)
    return true;

  const Bitboard defKing = pos.getPiece(~atkSide, KING);

  if (!(defKing & EdgeSquares))
    return true;

  const Square defKingSq = squareNo(defKing);
  const Square atkKingSq = squareNo(pos.getPiece(atkSide, KING));

  if (chebyshevDistance(atkKingSq, defKingSq) != 2)
    return true;

  // A knight's own square counts as covered even if the king could take it.
  // That can only overstate how trapped the king is, so it errs towards not
  // claiming a draw.
  const Bitboard knights = pos.getPiece(atkSide, KNIGHT);
  const Bitboard covered = plt::kingMasks[atkKingSq]
                         | plt::knightMasks[lsbIndex(knights)]
                         | plt::knightMasks[msbIndex(knights)]
                         | knights;

  return popCount(plt::kingMasks[defKingSq] & ~covered) > 1;
}

template <>
inline bool
Endgame<Endgames::KBBK>(const EgBoard& pos)
{
  // A bishop apiece (KBKB): drawn unless a king is cornered by its own bishop.
  if (pos.count<WHITE, BISHOP>() == 1)
  {
    return !(cornerBlockedByOwnPiece(pos.piece<WHITE, KING>(), pos.piece<WHITE, BISHOP>())
          or cornerBlockedByOwnPiece(pos.piece<BLACK, KING>(), pos.piece<BLACK, BISHOP>()));
  }

  const Bitboard bishops = pos.piece<WHITE, BISHOP>() | pos.piece<BLACK, BISHOP>();
  return !((bishops & WhiteSquares) and (bishops & BlackSquares));
}

template <>
inline bool
Endgame<Endgames::KBNK>(const EgBoard& pos)
{
  // A minor apiece (KBKN): drawn unless a king is cornered by its own minor.
  if (pos.count<WHITE, ALL>() == 1)
  {
    const Bitboard wMinor = pos.piece<WHITE, BISHOP>() | pos.piece<WHITE, KNIGHT>();
    const Bitboard bMinor = pos.piece<BLACK, BISHOP>() | pos.piece<BLACK, KNIGHT>();

    return !(cornerBlockedByOwnPiece(pos.piece<WHITE, KING>(), wMinor)
          or cornerBlockedByOwnPiece(pos.piece<BLACK, KING>(), bMinor));
  }

  // Bishop and knight against a bare king: won, unless the bare king is to
  // move and can take an undefended minor next to it. The attack masks ignore
  // every blocker except the attacking king, so they can only overstate the
  // defence, which errs towards not claiming a draw.
  const Color side = pos.count<WHITE, ALL>() > 0 ? BLACK : WHITE;

  if (side != pos.color)
    return false;

  const Bitboard emyKing   = pos.getPiece(~side, KING);
  const Square   kingSq    = squareNo(pos.getPiece(side, KING));
  const Square   emyKingSq = squareNo(emyKing);

  const Bitboard bishopMask = attackSquares<BISHOP>(squareNo(pos.getPiece(~side, BISHOP)), emyKing);
  const Bitboard knightMask = attackSquares<KNIGHT>(squareNo(pos.getPiece(~side, KNIGHT)), emyKing);

  const Bitboard hanging =
    pos.getPiece(~side, ALL) & ~plt::kingMasks[emyKingSq] & ~bishopMask & ~knightMask;

  return plt::kingMasks[kingSq] & hanging;
}


template <>
bool
Endgame<Endgames::KRBK>(const EgBoard& pos)
{
  if ((pos.count<WHITE, ALL>() == 2) or (pos.count<BLACK, ALL>() == 2))
    return false;

  const Color side = pos.count<WHITE, BISHOP>() ? WHITE : BLACK;
  const Color emySide = ~side;
  const int defToMove = side == pos.color;

  const Bitboard occupied = pos.all();
  const Bitboard    king = pos.getPiece(side   , KING  );
  const Bitboard  bishop = pos.getPiece(side   , BISHOP);
  const Bitboard emyKing = pos.getPiece(emySide, KING  );
  const Bitboard    rook = pos.getPiece(emySide, ROOK  );

  const Square    kingSq = squareNo(king   );
  const Square  bishopSq = squareNo(bishop );
  const Square emyKingSq = squareNo(emyKing);
  const Square    rookSq = squareNo(rook   );

  const int kingR    = kingSq >> 3;
  const int kingF    = kingSq  & 7;
  const int bishopR  = bishopSq >> 3;
  const int bishopF  = bishopSq  & 7;
  const int emyKingR = emyKingSq >> 3;
  const int emyKingF = emyKingSq & 7;
  const int rookR    = rookSq >> 3;
  const int rookF    = rookSq   & 7;

  // Distances used by the KRKB draw rules.
  const int distBtwKings       = chebyshevDistance(kingSq   , emyKingSq);
  const int distBtwKingAndBish = chebyshevDistance(kingSq   , bishopSq );
  const int atkKingBishD       = chebyshevDistance(emyKingSq, bishopSq );
  const int kingRookD          = chebyshevDistance(kingSq   , rookSq   );
  const int atkKingRookD       = chebyshevDistance(emyKingSq, rookSq   );

  const Bitboard bishopMask  = attackSquares<BISHOP>(bishopSq , king   );
  const Bitboard rookMask    = attackSquares< ROOK >(rookSq   , emyKing);
  const Bitboard kingMask    = attackSquares< KING >(kingSq   , 0);
  const Bitboard emyKingMask = attackSquares< KING >(emyKingSq, 0);
  const Bitboard rookDiag    = attackSquares<BISHOP>(rookSq   , 0);

  // rookHitsBishop ignores blockers; rookOnBish uses the current position.
  const Bitboard rookNow    = attackSquares<ROOK>(rookSq, occupied);
  const bool rookHitsBishop = attackSquares<ROOK>(rookSq, 0) & bishop;
  const bool rookOnBish     = rookNow & bishop;

  // Positions known to be decided.

  // Pinned bishop that cannot be saved. A rook that checks the king along
  // another line doesn't pin it.
  if (xrays<ROOK>(rookSq, occupied, bishop, king) and !(rookNow & king) and
     (distBtwKingAndBish > 2) and
     (!defToMove or (distBtwKings > 2))
  ) return false;

  // If the bishop can be taken immediately, defer to search.
  if (!defToMove and (rookMask & bishop) and !(kingMask & bishop))
    return false;

  if (defToMove and (rookDiag & emyKing))
  {
    const Bitboard mask = attackSquares<BISHOP>(emyKingSq, 0) & rookDiag;
    if (((mask & bishopMask) & ~emyKingMask) and (king & ~rookMask))
      return true;
  }

  if ((rookMask & king) and (rookMask & bishop))
  {
    const Bitboard inBtwMask = rookNow & attackSquares<ROOK>(kingSq, occupied);

    if (!((bishopMask | bishop) & inBtwMask) and (distBtwKingAndBish > 2))
      return false;
  }

  // Bishop aligned with its king on a rank or file.
  if (!defToMove and (distBtwKingAndBish > 2) and
     ((kingR == bishopR) or (kingF == bishopF)))
  {
    const bool onRank   = kingR == bishopR;
    const int  kingPerp = onRank ? kingF    : kingR;
    const int  rookPerp = onRank ? rookF    : rookR;
    const int  atkPerp  = onRank ? emyKingF : emyKingR;

    const Bitboard inBtwMask =
      attackSquares<ROOK>(kingSq, 0) & attackSquares<ROOK>(bishopSq, 0);

    if ((atkPerp != rookPerp) and
       (abs(kingPerp - rookPerp) > 1) and
       (emyKing & ~inBtwMask) and
      !(bishopMask & emyKing)
    ) return false;
  }

  // Known KRKB draw positions.

  const int kingEdgeD   = std::min(std::min(kingR, 7 - kingR),
                                   std::min(kingF, 7 - kingF));
  const int bishopEdgeD = std::min(std::min(bishopR, 7 - bishopR),
                                   std::min(bishopF, 7 - bishopF));
  const bool offRim = (kingEdgeD >= 1) and (bishopEdgeD >= 1);

  // Whether the defending king is on the bishop's colour.
  const bool kingOnBishColour =
    bool(king & WhiteSquares) == bool(bishop & WhiteSquares);

  // Distance from the defending king to the bishop-coloured corners.
  const Bitboard badCorners =
    CornerSquares & ((bishop & WhiteSquares) ? WhiteSquares : BlackSquares);

  int badCornerD = 8;
  for (Bitboard c = badCorners; c != 0; c &= c - 1)
    badCornerD = std::min(badCornerD, chebyshevDistance(kingSq, squareNo(c & -c)));

  // Draws where the bishop is protected by its king.
  if (distBtwKingAndBish <= 2)
  {
    const bool bishopBeside = distBtwKingAndBish == 1;

    // Kings are too far apart for the rook to break through.
    if (distBtwKings > 4)
      return true;

    // Both pieces are away from the rim and the attacking king is not close.
    if (offRim and (atkKingBishD >= 3))
      return true;

    if (offRim and bishopBeside and (atkKingBishD == 2))
      return true;

    // Attacking king is far away, except for the edge-trap case.
    if ((atkKingBishD >= 5) and
       !(!defToMove and rookHitsBishop and (kingEdgeD == 0)))
      return true;

    // Additional draw conditions at distance 4.
    if ((atkKingBishD == 4) and
       ((defToMove and !rookHitsBishop) or (!bishopBeside and !kingOnBishColour)))
      return true;

    // Bishop beside its king and away from the bad corners.
    if (bishopBeside and (badCornerD >= 2) and
       ((atkKingBishD == 4) or (atkKingBishD == 5) or
        (defToMove and (atkKingBishD == 3))))
      return true;

    // Draw when the kings are four squares apart in the listed configurations.
    if (!bishopBeside and (distBtwKings == 4) and
       ((kingEdgeD >= 1) or (defToMove and !kingOnBishColour)))
      return true;

    // Bishop beside its king and the rook is not attacking it.
    if (bishopBeside and (kingEdgeD >= 1) and !rookHitsBishop and
       (distBtwKings > 2 + !defToMove))
      return true;

    if (bishopBeside and defToMove and (distBtwKings == 4) and (kingRookD > 1))
      return true;
  }

  // Draws with the bishop three squares from the defending king.
  if (distBtwKingAndBish == 3)
  {
    if (kingOnBishColour and !rookHitsBishop and (atkKingBishD > 3))
      return true;

    if (defToMove and (distBtwKings >= 5) and
       (!kingOnBishColour or (kingRookD > 3)))
      return true;
  }

  // Loose bishop with both coordinates separated from the king.
  if ((kingEdgeD >= 1) and !rookHitsBishop and (atkKingBishD > 3) and
     (abs(kingR - bishopR) > 1) and (abs(kingF - bishopF) > 1))
    return true;

  // Draws based on the defending king's distance from the edge.
  if (defToMove and !rookOnBish and (kingEdgeD >= 2) and
     ((atkKingBishD == 2) or (atkKingBishD == 3) or (atkKingBishD >= 6)))
    return true;

  // Defending king is close to the rook while the attacking king is too far away.
  if (kingRookD == 1)
  {
    if (defToMove ? ((distBtwKings > 2) or (atkKingRookD > 1))
                  : ((distBtwKings > 4) or (atkKingRookD > 3)))
      return true;
  }
  else if (defToMove and (kingRookD == 2) and
          ((distBtwKings > 4) or (!rookHitsBishop and (atkKingRookD > 2))))
    return true;

  // Legal escape squares for the defending king.
  const Bitboard rookThruKing = attackSquares<ROOK>(rookSq, occupied ^ king);
  Bitboard kingEsc = kingMask & ~emyKingMask & ~bishop & ~rookThruKing;
  if (emyKingMask & rook) kingEsc &= ~rook;

  const int kingMob = popCount(kingEsc);

  // kingMob is used as a discrete draw classifier.
  if (defToMove)
  {
    switch (kingMob)
    {
      // Limited escape squares away from the bad corners.
      case 0: case 1: case 2:
        if ((distBtwKings >= 3) and (badCornerD >= 2) and (badCornerD <= 6))
          return true;
        if ((kingMob == 1) and (distBtwKings >= 5))
          return true;
        break;

      // Three escape squares: safe-corner or distant-bad-corner cases.
      case 3:
        if ((badCornerD == 7) and (distBtwKings >= 3))
          return true;
        if ((badCornerD == 0) and (distBtwKings >= 6))
          return true;
        break;

      case 4:
        if (!rookOnBish and (atkKingBishD >= 6))
          return true;
        break;

      // Defending king on the rim with all escape squares available.
      case 5:
        if ((distBtwKings >= 5) or
           ((distBtwKings == 4) and (badCornerD != 4)))
          return true;
        if (!rookOnBish and (atkKingBishD >= 4))
          return true;
        break;

      // King one step from the rim with two escape squares removed.
      case 6:
        if (!rookOnBish and ((atkKingBishD >= 5) or (atkKingBishD == 2)))
          return true;
        break;

      // King away from the rim with most escape squares available.
      case 7:
        if (!rookOnBish)
          return true;
        break;

      case 8:
        if (!rookOnBish)
          return true;
        if ((distBtwKings >= 4) or
           ((distBtwKings == 3) and (badCornerD >= 2)))
          return true;
        break;
    }
  }

  return false;
}

// KR vs KN. Checked against the 4-man oracle (`egvalidate pieces Rn oracle`): it
// claims 4,461,108 of the 8,370,088 draws (53%) with no false draw, in both
// colourings. The defender holds when the knight stays near its king and the
// kings are apart; the other claims look one move ahead into those positions.
// KRNK (both men on one side) falls through to search.
//
// Notation: d = defender (the knight's side), a = attacker (the rook's side);
// k/r/n = king/rook/knight, so `dn` is the knight's square and `dnB` its bitboard.
template <>
bool
Endgame<Endgames::KRNK>(const EgBoard& pos)
{
  if ((pos.count<WHITE, ALL>() == 2) or (pos.count<BLACK, ALL>() == 2))
    return false;

  const Color def = pos.count<WHITE, KNIGHT>() ? WHITE : BLACK;
  const Color atk = ~def;
  const bool defToMove = pos.color == def;

  const Bitboard akB = pos.getPiece(atk, KING  );
  const Bitboard arB = pos.getPiece(atk, ROOK  );
  const Bitboard dkB = pos.getPiece(def, KING  );
  const Bitboard dnB = pos.getPiece(def, KNIGHT);

  const Square ak = squareNo(akB), ar = squareNo(arB);
  const Square dk = squareNo(dkB), dn = squareNo(dnB);
  const Bitboard occ = pos.all();

  const Bitboard akAtt = attackSquares<KING  >(ak, 0);
  const Bitboard arAtt = attackSquares<ROOK  >(ar, occ);
  const Bitboard dkAtt = attackSquares<KING  >(dk, 0);
  const Bitboard dnAtt = attackSquares<KNIGHT>(dn, 0);

  // The side to move is in check.
  const bool chk = defToMove ? (arAtt & dkB) : (dnAtt & akB);

  // The defender hits the rook and the attacking king does not guard it.
  const bool rHang = (dkAtt | dnAtt) & arB & ~akAtt;

  // The rook pins the knight to its king.
  const bool nPin = xrays<ROOK>(ar, occ, dnB, dkB);

  // A knight move to an empty square that hits both the attacking king and rook.
  const bool nFork = knightForkSquares(dn, ak, ar, occ);

  // The attacking king stands between its rook and the knight, and the knight's
  // king is behind the knight on the same line: a king step pins the knight.
  const bool discPin = not (arAtt & dnB) and xrays<ROOK>(ar, occ ^ akB, dnB, dkB);

  const int kingsD = chebyshevDistance(ak, dk);
  const int dkND   = chebyshevDistance(dk, dn);
  const int akND   = chebyshevDistance(ak, dn);
  const int dkEdge = edgeDistance(dk);
  const int nEdge  = edgeDistance(dn);

  // The defender to move takes the rook: KN vs K or K vs K.
  if (defToMove and rHang)
    return true;

  // The knight forks the king and rook. A rook can't take a knight that hits it,
  // so the rook is lost.
  if (defToMove and not chk and not nPin and nFork)
    return true;

  // The knight stands within two squares of its king and the kings are far
  // enough apart: further when the defending king is on the edge or in a corner,
  // and further with the attacker to move. When the attacking king is four or
  // more squares from the knight, the kings may be one square closer (not with
  // the defending king in a corner).
  constexpr int minKingsD[2][3] = {
    { 3, 5, 6 },   // attacker to move: king off the edge, on the edge, in a corner
    { 3, 4, 5 },   // defender to move
  };
  const int  dkPlace = (dkB & CornerSquares) ? 2 : (dkEdge == 0) ? 1 : 0;
  const bool akFar   = akND >= 4 and dkPlace < 2;
  if (not chk and dkND <= 2 and kingsD >= minKingsD[defToMove][dkPlace] - akFar)
  {
    // With the defender to move, the knight is lost only when it is pinned and
    // the attacking king hits it too.
    if (defToMove and not (nPin and (akAtt & dnB)))
      return true;

    // With the attacker to move, the knight is lost when it is loose (hit and
    // unguarded), pinned, in a corner, or about to be pinned by a king step (the
    // attacking king stands between its rook and the knight, on the line to the
    // knight's king). Also lost: the knight on the edge two squares from its king,
    // with the kings three apart and the attacking king near the knight.
    const bool loose   = (akAtt | arAtt) & dnB & ~dkAtt;
    const bool nCorner = dnB & CornerSquares;
    const bool edgeNet = nEdge == 0 and dkND == 2 and kingsD == 3 and not akFar;
    if (not defToMove and not (loose or nPin or nCorner or discPin or edgeNet))
      return true;
  }

  // The knight checks the attacking king and neither the rook nor the king can
  // take it, so the king must step away. The defender holds when every step
  // reaches a position this recognizer already calls drawn.
  if (not defToMove and chk and not ((arAtt | (akAtt & ~dkAtt)) & dnB))
  {
    const Bitboard steps = akAtt & ~arB & ~dkAtt & ~dnAtt;
    bool allDrawn = steps != 0;
    for (Bitboard t = steps; t != 0 and allDrawn; t &= t - 1)
      allDrawn = isTheoreticalDraw(pos.play(ak, squareNo(t & -t)));
    if (allDrawn)
      return true;
  }

  // The defender to move holds when one of its moves reaches a position this
  // recognizer already calls drawn, with the attacker to move.
  if (defToMove)
  {
    const auto legal = [&] (const EgBoard& next) {
      const Bitboard k = next.getPiece(def, KING);
      const Bitboard r = next.getPiece(atk, ROOK);
      return not (attackSquares<KING>(squareNo(k), 0) & akB)
         and not (r and (attackSquares<ROOK>(squareNo(r), next.all()) & k));
    };

    // Knight checks are left out: the rule above that claims a position with the
    // attacker in check looks ahead again, so they would only recurse.
    const Bitboard nMoves = nPin ? 0 : dnAtt & ~dkB & ~attackSquares<KNIGHT>(ak, 0);
    for (Bitboard t = nMoves; t != 0; t &= t - 1)
    {
      const EgBoard next = pos.play(dn, squareNo(t & -t));
      if (legal(next) and isTheoreticalDraw(next))
        return true;
    }

    for (Bitboard t = dkAtt & ~dnB & ~akAtt; t != 0; t &= t - 1)
    {
      const EgBoard next = pos.play(dk, squareNo(t & -t));
      if (legal(next) and isTheoreticalDraw(next))
        return true;
    }
  }

  return false;
}

// KR vs KR helpers. Notation: u = the side to move ("us"), t = the other side
// ("them"); k/r = king/rook, so `tr` is the other side's rook square and `trB`
// its bitboard.

// The side to move wins at once: it takes the other rook and KR vs K is won (the
// king can't take back, isn't stalemated and can't take the rook next), a rook
// check skewers the other rook, or a rook move mates. False when a rook is
// missing.
static bool
krkrWinsAtOnce(const EgBoard& pos)
{
  const Color us = pos.color, them = ~us;

  const Bitboard ukB = pos.getPiece(us  , KING);
  const Bitboard urB = pos.getPiece(us  , ROOK);
  const Bitboard tkB = pos.getPiece(them, KING);
  const Bitboard trB = pos.getPiece(them, ROOK);
  if (not urB or not trB)
    return false;

  const Square uk = squareNo(ukB), ur = squareNo(urB);
  const Square tk = squareNo(tkB), tr = squareNo(trB);
  const Bitboard occ = pos.all();

  const Bitboard ukAtt = attackSquares<KING>(uk, 0);
  const Bitboard urAtt = attackSquares<ROOK>(ur, occ);
  const Bitboard tkAtt = attackSquares<KING>(tk, 0);

  // After taking, their king needs a move, and taking our rook mustn't be one.
  const auto takeWins = [&] (Square from) {
    const EgBoard  next = pos.play(from, tr);
    const Bitboard nkB  = next.getPiece(us, KING), nrB = next.getPiece(us, ROOK);
    const Bitboard flights = tkAtt & ~attackSquares<KING>(squareNo(nkB), 0)
                                   & ~attackSquares<ROOK>(squareNo(nrB), next.all() ^ tkB);
    return flights != 0 and not (flights & nrB);
  };

  if ((urAtt & trB & ~tkAtt) and takeWins(ur)) return true;
  if ((ukAtt & trB & ~tkAtt) and takeWins(uk)) return true;

  for (Bitboard t = urAtt & ~occ; t != 0; t &= t - 1)
  {
    const Square   s  = squareNo(t & -t);
    const Bitboard sB = t & -t;
    const Bitboard o  = occ ^ urB ^ sB;
    const Bitboard sAtt  = attackSquares<ROOK>(s , o);
    const Bitboard trAtt = attackSquares<ROOK>(tr, o);

    if (not (sAtt & tkB) or (trAtt & ukB))
      continue;   // no check, or our king is left in check

    const Bitboard xray    = attackSquares<ROOK>(s, o ^ tkB);
    const Bitboard flights = tkAtt & ~(ukAtt | xray | trB);
    const bool kTakes = tkAtt & sB & ~ukAtt;
    const bool rTakes = trAtt & sB;
    const bool rBlock = trAtt & sAtt & attackSquares<ROOK>(tk, o);

    // Mate.
    if (not (kTakes or rTakes or rBlock) and flights == 0)
      return true;

    // A skewer: their rook stands behind the king, and the king can't step next
    // to it to take back (or our king guards the square).
    if (not (kTakes or rTakes) and (xray & trB)
        and ((ukAtt & trB) or not (flights & attackSquares<KING>(tr, 0))))
      return true;
  }

  return false;
}

// Calls `f(next)` for each legal move of the side to move, and stops early when
// `f` returns false. Returns false if it was stopped.
template <typename F>
static bool
krkrEachMove(const EgBoard& pos, F f)
{
  const Color us = pos.color, them = ~us;

  const Bitboard ukB = pos.getPiece(us  , KING);
  const Bitboard urB = pos.getPiece(us  , ROOK);
  const Bitboard tkB = pos.getPiece(them, KING);
  const Bitboard trB = pos.getPiece(them, ROOK);

  const Square uk = squareNo(ukB), ur = squareNo(urB), tr = squareNo(trB);
  const Bitboard occ   = pos.all();
  const Bitboard tkAtt = attackSquares<KING>(squareNo(tkB), 0);

  for (Bitboard t = attackSquares<KING>(uk, 0) & ~urB & ~tkAtt; t != 0; t &= t - 1)
  {
    const Bitboard sB = t & -t;
    if ((sB & trB) or not (attackSquares<ROOK>(tr, occ ^ ukB) & sB))
      if (not f(pos.play(uk, squareNo(sB))))
        return false;
  }

  for (Bitboard t = attackSquares<ROOK>(ur, occ) & ~(ukB | tkB); t != 0; t &= t - 1)
  {
    const Bitboard sB = t & -t;
    if ((sB & trB) or not (attackSquares<ROOK>(tr, (occ ^ urB) | sB) & ukB))
      if (not f(pos.play(ur, squareNo(sB))))
        return false;
  }

  return true;
}

// Every legal move of the side to move leaves the other side an immediate win,
// and there is at least one such move.
static bool
krkrEveryMoveLoses(const EgBoard& pos)
{
  bool any = false;
  const bool all = krkrEachMove(pos, [&] (const EgBoard& next) {
    any = true;
    return krkrWinsAtOnce(next);
  });
  return any and all;
}

// The side to move gives a check (a rook move, or a king step that uncovers the
// rook) after which every reply leaves it an immediate win.
static bool
krkrWinsByCheck(const EgBoard& pos)
{
  const Color us = pos.color, them = ~us;
  const Bitboard tkB = pos.getPiece(them, KING);

  bool wins = false;
  krkrEachMove(pos, [&] (const EgBoard& next) {
    const Bitboard urB = next.getPiece(us, ROOK);
    if (urB and next.getPiece(them, ROOK)
        and (attackSquares<ROOK>(squareNo(urB), next.all()) & tkB))
      wins = krkrEveryMoveLoses(next);
    return not wins;
  });
  return wins;
}

// Quiet draws: the side to move is out of check, has no immediate win and no
// winning check, and the kings are at least two squares apart. Kings two or
// three apart have their own exceptions (mating nets).
static bool
krkrQuietDraw(const EgBoard& pos)
{
  const Color us = pos.color, them = ~us;

  const Bitboard ukB = pos.getPiece(us  , KING);
  const Bitboard urB = pos.getPiece(us  , ROOK);
  const Bitboard tkB = pos.getPiece(them, KING);
  const Bitboard trB = pos.getPiece(them, ROOK);
  if (not urB or not trB)
    return false;

  const Square uk = squareNo(ukB), ur = squareNo(urB);
  const Square tk = squareNo(tkB), tr = squareNo(trB);
  const Bitboard occ = pos.all();

  const Bitboard ukAtt = attackSquares<KING>(uk, 0);
  const Bitboard tkAtt = attackSquares<KING>(tk, 0);
  const Bitboard trAtt = attackSquares<ROOK>(tr, occ);
  if (trAtt & ukB)
    return false;

  const int  kingsD  = chebyshevDistance(uk, tk);
  const bool ukEdge  = ukB & EdgeSquares;
  const bool tkEdge  = tkB & EdgeSquares;
  if (kingsD < 2)
    return false;

  // Kings two apart, their king off the edge. Not when the rooks hit each other,
  // our rook hangs with our king on the edge, our king is in a corner a knight's
  // jump from theirs, or our king is on the edge with its rook beside it and
  // their king facing it (a back-rank net).
  if (kingsD == 2)
  {
    const bool rHit  = attackSquares<ROOK>(ur, occ) & trB;
    const bool uHang = (tkAtt | trAtt) & urB & ~ukAtt;
    const bool kJump = attackSquares<KNIGHT>(uk, 0) & tkB;
    const bool kOpp  = (uk >> 3) == (tk >> 3) or (uk & 7) == (tk & 7);

    if (tkEdge or rHit or (uHang and ukEdge) or ((ukB & CornerSquares) and kJump)
        or (ukEdge and kOpp and (ukAtt & urB)))
      return false;
  }

  // Kings three apart, their king on an edge square next to its rook and ours off
  // the edge: a mating net.
  if (kingsD == 3 and tkEdge and not (tkB & CornerSquares) and not ukEdge and (tkAtt & trB))
    return false;

  return not krkrWinsAtOnce(pos) and not krkrWinsByCheck(pos);
}

// KR vs KR. Checked against the 4-man oracle (`egvalidate pieces Rr oracle`): it
// claims 6,904,932 of the 7,569,032 draws (91%) with no false draw. The game is
// drawn unless a tactic decides it: the side to move takes a loose rook, skewers
// it, mates or wins with a check, or the kings stand close in a mating net.
// KRRK falls through to search.
template <>
bool
Endgame<Endgames::KRKR>(const EgBoard& pos)
{
  if ((pos.count<WHITE, ALL>() == 2) or (pos.count<BLACK, ALL>() == 2))
    return false;

  if (krkrQuietDraw(pos))
    return true;

  // In check, a reply reaches a quiet draw and every other reply reaches one too
  // or loses at once (it leaves the other side an immediate win).
  const Bitboard ukB = pos.getPiece( pos.color, KING);
  const Bitboard trB = pos.getPiece(~pos.color, ROOK);
  if (not (attackSquares<ROOK>(squareNo(trB), pos.all()) & ukB))
    return false;

  bool drawn = false;
  const bool all = krkrEachMove(pos, [&] (const EgBoard& next) {
    if (krkrQuietDraw(next))
      return drawn = true;
    return krkrWinsAtOnce(next);
  });
  return drawn and all;
}

template <>
inline bool
Endgame<Endgames::KPNK>(const EgBoard& pos)
{
  // Evaluate from the knight side.
  const Color side2move = pos.color;
  const Color side    = pos.count<WHITE, KNIGHT>() ? WHITE : BLACK;   // knight (defender)
  const Color emySide = ~side;                                        // pawn (attacker)

  const int defToMove = int(side2move == side);

  const Bitboard myKing  = pos.getPiece(side   , KING);
  const Bitboard emyKing = pos.getPiece(emySide, KING);

  if (pos.count<WHITE, ALL>() == 1)
  {
    const Bitboard knight = pos.getPiece(side,    KNIGHT);
    const Bitboard pawn   = pos.getPiece(emySide, PAWN  );

    const Square  knightSq = squareNo(knight );
    const Square    pawnSq = squareNo(pawn   );
    const Square  myKingSq = squareNo(myKing );
    const Square emyKingSq = squareNo(emyKing);

    const int    pawnR = pawnSq >> 3;
    const int    pawnF = pawnSq  & 7;
    // Pawn rank relative to the attacking side.
    const int  pawnRel = (emySide == WHITE) ? pawnR + 1 : 8 - pawnR;

    Bitboard kingCapMask    = attackSquares<KING>(myKingSq , 0);
    Bitboard emyKingCapMask = attackSquares<KING>(emyKingSq, 0);

    // If the knight can be taken immediately, defer to search.
    if (!(defToMove) and
      ((knight & plt::pawnCaptureMasks[emySide][pawnSq]) or
      (knight & emyKingCapMask & ~kingCapMask))
    ) return false;

    const Square promoSq = static_cast<Square>((emySide == WHITE ? 56 : 0) + pawnF);
    const int defKingPromoDist = chebyshevDistance(myKingSq, promoSq);
    const int defKingPawnDist  = chebyshevDistance(myKingSq, pawnSq);
    const int  knightPawnDist  = chebyshevDistance(knightSq, pawnSq);
    const int ruleOfSquareIndex = getRuleOfSquareIndex(pos, emySide, pawnSq) + 8 * (2 * emySide - 1);

    const auto pawnOnFileAH = (pawn & FileAH) != 0;
    const auto kingInROS = (plt::ruleOfSquares[emySide][ruleOfSquareIndex] & myKing) != 0;
    const auto legalKnightSquares = (plt::knightMasks[knightSq] &
      ~(attackSquares<KING>(emyKingSq, 0) | plt::pawnCaptureMasks[emySide][pawnSq])) != 0;

    // Known draw conditions.

    // Defender king occupies the promotion square.
    if (defKingPromoDist == 0)
      return true;

    // Unadvanced pawn with the defending pieces close enough to hold.
    if (pawnRel <= 3 and defKingPromoDist <= 3 and
        std::min(defKingPawnDist, knightPawnDist) <= 3)
      return true;

    // Defender to move with a safe knight move and a non-rook pawn.
    if (pawnRel <= 3 and defToMove and legalKnightSquares and not pawnOnFileAH)
      return true;

    // Same holding pattern one rank further advanced, with the king inside the
    // rule of the square.
    if (pawnRel <= 4 and defToMove and legalKnightSquares and not pawnOnFileAH and kingInROS)
      return true;

    const int  knightF =  knightSq & 7;
    const int emyKingF = emyKingSq & 7;
    if ((plt::knightMasks[knightSq] & (1ULL << promoSq)) and
        (pawn & relativeRank[emySide][7] or (legalKnightSquares and defToMove)) and
       !(pawn & FileAH) and
       !((chebyshevDistance(emyKingSq, pawnSq) <= 1 + !defToMove) and ((knightF - pawnF) * (emyKingF - pawnF) >= 0))
    ) return true;

    // The knight forks the attacking king and the pawn.
    if (defToMove and
       knightForkSquares(knightSq, emyKingSq, pawnSq, pos.all()) and
      !(myKing & plt::pawnCaptureMasks[emySide][pawnSq]) and
       (emyKing & ~CornerSquares)
    ) return true;

    if ((defKingPromoDist == 1) and
        (chebyshevDistance(emyKingSq, promoSq) > 2)
    ) return true;

    if (kingInROS and
       ((chebyshevDistance(emyKingSq, pawnSq) > 5)
     or (chebyshevDistance(emyKingSq, promoSq) == 4))
    ) return true;

    // Unrecognized positions are handled by search.
    return false;
  }

  return false;
}

template <>
inline bool
Endgame<Endgames::KPRK>(const EgBoard& pos)
{
  // Look from the pawn side, which defends against the rook. Like KPQK and
  // KPNK, this covers both ways the material can be split. KP vs KR has the
  // draw rules. KPR vs K is a win and falls through to return false.
  const Color side    = pos.count<WHITE, PAWN>() ? WHITE : BLACK;   // pawn (defender)
  const Color emySide = ~side;                                      // rook (attacker)
  const auto defToMove = int(side == pos.color);

  const Bitboard  myKing = pos.getPiece(side   , KING);
  const Bitboard emyKing = pos.getPiece(emySide, KING);

  if (pos.count<WHITE, ALL>() == 1)
  {
    const Bitboard pawn = pos.getPiece(side   , PAWN);
    const Bitboard rook = pos.getPiece(emySide, ROOK);

    const Square    pawnSq = squareNo(pawn   );
    const Square    rookSq = squareNo(rook   );
    const Square  myKingSq = squareNo(myKing );
    const Square emyKingSq = squareNo(emyKing);

    const int pawnR = pawnSq >> 3;
    const int pawnF = pawnSq &  7;

    const Square promoSq = static_cast<Square>((side == WHITE ? 56 : 0) + pawnF);

    // Rank of the pawn counted from its own side, 2..7 (7 == one step from promoting).
    const int pawnRel = (side == WHITE) ? (pawnR + 1) : (8 - pawnR);

    const Bitboard rookMask  = attackSquares<ROOK>(rookSq, emyKing) & ~emyKing;
    const Bitboard kingCapMask    = attackSquares<KING>(myKingSq , 0);
    const Bitboard emyKingCapMask = attackSquares<KING>(emyKingSq, 0);

    // If the side to move can take the other side's man, leave it to search.
    // Unlike KPBK, KRBK and KPNK there is one check for each side, because both
    // men can hang here: the defender has a pawn, and the rook can be taken.
    if (defToMove and
       ((rook & plt::pawnCaptureMasks[side][pawnSq]) or
       (rook & kingCapMask & ~emyKingCapMask))
    ) return false;

    if (!defToMove and
      (rookMask & pawn & ~kingCapMask)
    ) return false;

    const int dkPromoD = chebyshevDistance(myKingSq , promoSq);
    const int akPromoD = chebyshevDistance(emyKingSq, promoSq);
    const int  dkPawnD = chebyshevDistance(myKingSq ,  pawnSq);
    const int  akPawnD = chebyshevDistance(emyKingSq,  pawnSq);
    const int   rPawnD = chebyshevDistance(rookSq   ,  pawnSq);

    // Can the rook reach the promotion square's rank or file on a square that
    // neither the defending king nor the pawn covers? Used by two rules below.
    const Bitboard reachMask = plt::lineMasks[promoSq]
                             & ~(plt::kingMasks[myKingSq] | plt::pawnCaptureMasks[side][pawnSq]);
    const bool rookReach = (rookMask & reachMask) != 0;

    // Pawn one step from promoting, rook side to move. The defending king is
    // near the promotion square (dkPromoD <= 2) and the attacking king is too
    // far from the pawn to help (akPawnD >= 4). The rook has to give itself up
    // for the pawn, or let it promote and win the new queen with a skewer.
    // Either way only the kings are left.
    if (rookReach and defToMove == 0 and pawnRel == 7
        and dkPromoD <= 2 and akPawnD >= 4)
      return true;

    // The defending king is on or next to its own promotion square, with the
    // pawn two ranks behind. The king blocks its own pawn, so the pawn side
    // can't make progress, and the rook just waits. The akPawnD limit keeps the
    // attacking king too far away to win. It goes up with dkPromoD, because a
    // king one square off the promotion square needs an extra tempo.
    if (dkPromoD <= 1 and dkPawnD == 2 and akPawnD >= 5 + dkPromoD)
      return true;

    // The same blocking shape, limited by where the attacking king is instead
    // of its distance to the pawn: it is far from the promotion square
    // (akPromoD >= 6) and can't come back in time. The two rook exclusions are
    // where the pawn side breaks through. With rPawnD equal to akPromoD or two
    // less, the rook can't both give check and get back, and the pawn promotes.
    if (dkPromoD <= 1 and pawnRel == 6 and akPromoD >= 6
        and rPawnD != akPromoD and rPawnD != akPromoD - 2
    ) return true;

    if ((dkPromoD + chebyshevDistance(pawnSq, promoSq) < akPromoD) and
        (dkPawnD == 1) and (rPawnD > 1 + defToMove) and (pawnRel < 8 - defToMove)
        and (akPawnD > 3 + !defToMove) and ((myKingSq & 7) != (pawnSq & 7))
    ) {
      if (rookReach)
        return true;
    }

    return false;
  }

  return false;
}

// KBN vs KN. Mined against the 5-man oracle (`egvalidate pieces BNn oracle
// nocapgate`): apart from one knight fork, every clause is a box of feature
// thresholds that holds only draws over the whole call set. Together they claim
// 244,975,754 of the 583,817,814 draws (42%) with no false draw, in both
// colourings. The boxes are data-mined, so read them as "this region is drawn",
// not as chess rules.
// KBNN vs K fall through to search.
//
// Notation: d = defender (the lone knight's side), a = attacker (bishop + knight);
// k/b/n = king/bishop/knight, so `dk` is the defending king's square and `dkB` its
// bitboard.
template <>
bool
Endgame<Endgames::KBNKN>(const EgBoard& pos)
{
  const Color atk = pos.count<WHITE, BISHOP>() ? WHITE : BLACK;
  const Color def = ~atk;
  if (popCount(pos.getPiece(atk, KNIGHT)) != 1)
    return false;

  const bool defToMove = pos.color == def;

  const Bitboard akB = pos.getPiece(atk, KING  );
  const Bitboard abB = pos.getPiece(atk, BISHOP);
  const Bitboard anB = pos.getPiece(atk, KNIGHT);
  const Bitboard dkB = pos.getPiece(def, KING  );
  const Bitboard dnB = pos.getPiece(def, KNIGHT);

  const Square ak = squareNo(akB), ab = squareNo(abB), an = squareNo(anB);
  const Square dk = squareNo(dkB), dn = squareNo(dnB);
  const Bitboard occ = pos.all();

  const Bitboard akAtt = attackSquares<KING  >(ak, 0);
  const Bitboard abAtt = attackSquares<BISHOP>(ab, occ);
  const Bitboard anAtt = attackSquares<KNIGHT>(an, 0);
  const Bitboard dkAtt = attackSquares<KING  >(dk, 0);
  const Bitboard dnAtt = attackSquares<KNIGHT>(dn, 0);

  // Squares the attacker covers, the bishop seeing through the defending king (a
  // king cannot step back along the diagonal it is checked on).
  const Bitboard atkCover = akAtt | anAtt | attackSquares<BISHOP>(ab, occ ^ dkB);

  // The side to move is in check.
  const bool chk = defToMove ? ((abAtt | anAtt) & dkB) : (dnAtt & akB);

  // Attacking men (king included) that hit the defending knight.
  const int dnAtk = bool(akAtt & dnB) + bool(abAtt & dnB) + bool(anAtt & dnB);

  // Moves to squares the attacker does not cover, for the defending knight and king.
  const int dnSafe = popCount(dnAtt & ~dkB & ~atkCover);
  const int dkSafe = popCount(dkAtt & ~dnB & ~atkCover);

  // Defending king and knight share a diagonal -- the bishop's skewer line.
  const bool bLine = attackSquares<BISHOP>(dk, 0) & dnB;

  // Defending king's distance to the nearer corner of the bishop's colour (the
  // only corners KBN can mate in).
  const Bitboard bishCorners =
    CornerSquares & ((abB & WhiteSquares) ? WhiteSquares : BlackSquares);
  int dkCorner = 8;
  for (Bitboard c = bishCorners; c != 0; c &= c - 1)
    dkCorner = std::min(dkCorner, chebyshevDistance(dk, squareNo(c & -c)));

  const int dkKnD  = chebyshevDistance(dk, dn);
  const int akKnD  = chebyshevDistance(ak, dn);
  const int kingsD = chebyshevDistance(ak, dk);
  const int dkEdge = edgeDistance(dk);
  const int akEdge = edgeDistance(ak);

  if (defToMove)
  {
    // Attacking minors the defender hits and the attacker does not guard.
    const int aHang = bool((dkAtt | dnAtt) & abB & ~(akAtt | anAtt))
                    + bool((dkAtt | dnAtt) & anB & ~(akAtt | abAtt));

    // Flight squares of the attacking king. At 0 or 1 it is boxed in by its own
    // men and the defending knight can mate.
    const int akSafe = popCount(akAtt & ~(abB | anB) & ~(dkAtt | dnAtt));

    // The knight forks the attacking king and bishop from a square the attacking
    // knight doesn't cover; the king and bishop can't reach it. The king must
    // step away, and NxB leaves KN vs KN, or KN vs K after a recapture. The king
    // needs a square to step to, or the check is mate. The knight can't go when
    // its king is in check or when it is pinned. This adds 4.78M draws the
    // clauses below miss, and each guard is needed.
    if (not chk and not xrays<BISHOP>(ab, occ, dnB, dkB))
      for (Bitboard t = knightForkSquares(dn, ak, ab, occ) & ~anAtt; t != 0; t &= t - 1)
        if (akAtt & ~(abB | anB | dkAtt | attackSquares<KNIGHT>(squareNo(t & -t), 0)))
          return true;

    // The defender wins a loose minor (69.8M draws from the first clause alone,
    // 187.7M from all ten), or keeps both men close together with free squares
    // while the attacking king stands off.
    return (!chk and dnAtk <= 1 and aHang >= 1 and akSafe >= 1)
        or (!chk and dnAtk == 0 and dkKnD <= 4 and dkCorner <= 5 and kingsD >= 3
                 and dnSafe >= 3 and dkSafe >= 4 and akEdge <= 1)
        or (!chk and dnAtk == 0 and dkKnD <= 2 and akKnD >= 3
                 and dnSafe >= 2 and dkSafe >= 3 and akSafe >= 2)
        or (dnAtk == 0 and dkKnD <= 4 and kingsD >= 5 and dnSafe >= 3 and !bLine)
        or (!chk and dkKnD <= 6 and kingsD >= 4 and akKnD >= 2
                 and dnSafe >= 4 and dkSafe >= 5 and !bLine and akEdge <= 1)
        or (!chk and dnAtk == 0 and dkKnD <= 5 and dkCorner >= 2 and akKnD >= 6
                 and dnSafe >= 1 and dkSafe >= 4)
        or (dnAtk == 0 and dkKnD == 3 and dkCorner >= 2
                 and dnSafe >= 3 and dkSafe >= 4 and !bLine and akSafe >= 2)
        or ( chk and dnAtk == 0 and dkKnD <= 2 and dkCorner >= 2 and dkCorner <= 5
                 and dnSafe >= 2 and dkSafe >= 4 and !bLine and akEdge <= 2)
        or ( chk and dnAtk == 0 and dkKnD <= 5 and akKnD >= 6
                 and dnSafe >= 1 and dkSafe >= 4 and !bLine)
        or (!chk and aHang >= 1 and !bLine and akSafe >= 1);
  }

  // Attacker to move: forks and skewers are live, so the drawn regions are small
  // and mostly need the knight out of reach of the next move (52.5M draws).
  const bool dnDef = dkAtt & dnB;                                   // knight guarded by its king
  const int  bHit  = popCount(abAtt & ~occ                          // bishop moves that hit it
                            & attackSquares<BISHOP>(dn, occ & ~abB));
  const bool nHit  = anAtt & ~occ & attackSquares<KNIGHT>(dn, 0);   // a knight move hits it

  return (!nHit and dnAtk == 0 and dkKnD <= 3 and kingsD >= 4 and akKnD >= 5
               and dnSafe >= 1 and dkSafe >= 3 and !bLine)
      or (dnAtk == 0 and dkKnD <= 2 and dkEdge >= 1 and akKnD >= 3 and dnSafe >= 3 and !bLine)
      or (!nHit and !chk and dnAtk == 0 and dkKnD <= 4 and kingsD >= 6 and akKnD >= 3
               and dnSafe >= 2 and dkSafe >= 2 and !bLine and akEdge <= 1)
      or (bHit >= 1 and !nHit and dnAtk == 0 and dkKnD <= 4 and dkEdge >= 1
               and akKnD >= 5 and !bLine)
      or (dnDef and bHit <= 1 and !nHit and dnAtk == 0 and dkKnD <= 3
               and dkCorner >= 3 and dkCorner <= 5 and dkSafe >= 4 and bLine)
      or (dnAtk == 0 and dkKnD <= 2 and dkCorner <= 5 and kingsD >= 4
               and dnSafe >= 3 and dkSafe >= 4 and !bLine)
      or (dnDef and dnAtk <= 1 and dkEdge >= 1 and akKnD >= 3 and dkSafe >= 4 and !bLine)
      or (!nHit and dnAtk == 0 and dkKnD <= 3 and dkEdge >= 1 and kingsD >= 4
               and akKnD >= 3 and dnSafe >= 1 and !bLine);
}

// KRB vs KR. Checked against the 5-man oracle (`egvalidate pieces Rbr oracle`):
// it claims 220,605,512 of the 473,847,914 draws (47%) with no false draw, in
// both colourings. The early blocks rule out tactics and mates that decide the
// game; the claims after them are quiet regions, each with its own exceptions.
// KRRKB and KRRBK fall through to search.
//
// Notation: d = defender (the lone rook's side), a = attacker (rook + bishop);
// k/r/b = king/rook/bishop, so `dr` is the defending rook's square and `drB` its
// bitboard.
template <>
bool
Endgame<Endgames::KRBKR>(const EgBoard& pos)
{
  const Color atk = pos.count<WHITE, BISHOP>() ? WHITE : BLACK;
  const Color def = ~atk;
  if (popCount(pos.getPiece(atk, ROOK)) != 1)
    return false;

  const bool defToMove = pos.color == def;

  const Bitboard akB = pos.getPiece(atk, KING  );
  const Bitboard arB = pos.getPiece(atk, ROOK  );
  const Bitboard abB = pos.getPiece(atk, BISHOP);
  const Bitboard dkB = pos.getPiece(def, KING  );
  const Bitboard drB = pos.getPiece(def, ROOK  );

  const Square ak = squareNo(akB), ar = squareNo(arB), ab = squareNo(abB);
  const Square dk = squareNo(dkB), dr = squareNo(drB);
  const Bitboard occ = pos.all();

  const Bitboard akAtt = attackSquares<KING  >(ak, 0);
  const Bitboard arAtt = attackSquares<ROOK  >(ar, occ);
  const Bitboard abAtt = attackSquares<BISHOP>(ab, occ);
  const Bitboard dkAtt = attackSquares<KING  >(dk, 0);
  const Bitboard drAtt = attackSquares<ROOK  >(dr, occ);

  // The side to move is in check.
  const bool chk = defToMove ? ((arAtt | abAtt) & dkB) : (drAtt & akB);

  // Attacking pieces the defender hits and the attacker does not guard.
  const int aHang = bool((dkAtt | drAtt) & arB & ~(akAtt | abAtt))
                  + bool((dkAtt | drAtt) & abB & ~(akAtt | arAtt));

  // The rooks hit each other. A rook trade leaves KB vs K.
  const bool rHit = drAtt & arB;

  // The defending rook pins the bishop to its king.
  const bool bPin = xrays<ROOK>(dr, occ, abB, akB);

  // Squares the attacker covers against the defending king. The sliders see
  // through the king (it cannot step back along the line it is checked on).
  const Bitboard abThruDk = attackSquares<BISHOP>(ab, occ ^ dkB);
  const Bitboard atkCover = akAtt | attackSquares<ROOK>(ar, occ ^ dkB) | abThruDk;

  // Lines traced with the defending rook lifted off its square: the attacking
  // sliders' lines through it, and the defending rook's own lines once it has
  // taken the bishop (RxB).
  const Bitboard drLift = occ ^ drB;
  const Bitboard abLift = attackSquares<BISHOP>(ab, drLift);
  const Bitboard arLift = attackSquares<ROOK  >(ar, drLift);
  const Bitboard drOnB  = (drAtt & abB) ? attackSquares<ROOK>(ab, drLift) : 0;

  // Defending rook moves to squares where it is not lost: the bishop does not
  // hit it, and the attacking king and rook either do not hit it or the
  // defending king guards it (RxR KxR is KB vs K).
  const Bitboard drSafeSq = ~abLift & (~(akAtt | arLift) | dkAtt);
  const int drSafe = popCount(drAtt & ~dkB & drSafeSq);

  // Squares of the bishop's colour. A king off them can never be checked by it.
  const Bitboard abColour = (abB & WhiteSquares) ? WhiteSquares : BlackSquares;
  const bool dkOffB = not (dkB & abColour);

  const int kingsD = chebyshevDistance(ak, dk);
  const int dkEdge = edgeDistance(dk);
  const int akEdge = edgeDistance(ak);

  // Attacker to move and able to take the defending rook: 104M attacker wins
  // against 7M draws, so leave it to search. The bishop can't take while the
  // rook pins it, and the king can't take a rook its king guards.
  if (not defToMove)
  {
    const bool aCap = (arAtt & drB) or ((abAtt & drB) and not bPin)
                   or ((akAtt & drB) and not (dkAtt & drB));
    if (aCap)
      return false;
  }

  // The defending rook mates in one, or threatens a back-rank mate.
  if (defToMove and not chk and (rookMatesInOne<BISHOP>(pos) or backRankThreat(pos)))
    return false;

  // Skewer: the defender is in check and the bishop hits its rook, directly or
  // through the king. The king must step away and BxR leaves KRB vs K, unless the
  // rook takes the checking rook, the rook blocks next to its king (RxR KxR is
  // KB vs K), the king takes a loose piece, or the king steps next to a rook
  // nothing guards once the bishop has taken (BxR KxR is KB vs K).
  if (defToMove and chk and (abThruDk & drB))
  {
    const bool rookChk = (arAtt & dkB) and not (abAtt & dkB);   // only then can a block answer it
    const Bitboard dkMoves = dkAtt & ~drB & ~atkCover;

    const bool rTakes  = rHit and not (abLift & dkB);
    const bool rBlocks = rookChk and (drAtt & arAtt & attackSquares<ROOK>(dk, occ) & dkAtt);
    const bool kTakes  = dkMoves & (arB | abB);
    const bool kHitsR  = (dkMoves & attackSquares<KING>(ar, 0))
                     and not ((akAtt | attackSquares<BISHOP>(dr, occ ^ abB)) & arB);

    if (not (rTakes or rBlocks or kTakes or kHitsR))
      return false;
  }

  // The king must move and drop the rook: the defender is in check, the attacking
  // king hits the rook, and the rook can't answer the check. After any king step
  // KxR leaves KRB vs K, unless the king steps next to its rook, takes the
  // attacking rook, or steps next to an attacking rook that nothing guards once
  // the king has taken (KxR KxR is KB vs K).
  if (defToMove and chk and (akAtt & drB))
  {
    // Rook moves that leave no slider hitting the king (the one it takes aside).
    bool rMoves = false;
    for (Bitboard t = drAtt & ~(dkB | akB); t != 0 and not rMoves; t &= t - 1)
    {
      const Bitboard tB   = t & -t;
      const Bitboard occT = drLift | tB;
      const bool rChk = (attackSquares<ROOK  >(ar, occT) & dkB) and not (tB & arB);
      const bool bChk = (attackSquares<BISHOP>(ab, occT) & dkB) and not (tB & abB);
      rMoves = not (rChk or bChk);
    }

    const Bitboard dkMoves = dkAtt & ~drB & ~atkCover;

    // After KxR the king on dr doesn't guard the attacking rook, and the bishop
    // doesn't either, or the defending king takes the bishop on its way.
    const Bitboard nextToR = dkMoves & attackSquares<KING>(ar, 0);
    const bool bGuardsR    = attackSquares<BISHOP>(ab, occ ^ akB ^ dkB) & arB;

    const bool kGuards = dkMoves & attackSquares<KING>(dr, 0);
    const bool kTakesR = dkMoves & arB;
    const bool kHitsR  = not (attackSquares<KING>(dr, 0) & arB)
                     and ((nextToR & abB) or (nextToR and not bGuardsR));

    if (not (rMoves or kGuards or kTakesR or kHitsR))
      return false;
  }

  // RxB+ wins the rook: the defending king hits the attacking rook. The rook
  // takes the bishop with check, which removes the bishop's guard and drives the
  // attacking king away, and then the king takes the rook (KR vs K). The attacker
  // holds if it takes back, blocks next to its king (RxR KxR is K vs K), steps
  // next to its own rook, or steps next to the defending rook on a square the
  // defending king can't guard after KxR. `drOnB` and `arLift` are the rooks'
  // lines once RxB is played.
  const bool rxbChk = defToMove and (drOnB & akB) and not (arLift & dkB);   // check, and legal
  if (rxbChk and (dkAtt & arB))
  {
    const Bitboard akMoves = akAtt & ~(arB | abB | dkAtt | attackSquares<ROOK>(ab, drLift ^ akB));

    const bool kRetakes = (akAtt & abB) and not (dkAtt & abB);
    const bool rRetakes = arLift & abB;
    const bool rBlocks  = arLift & drOnB & attackSquares<ROOK>(ak, drLift) & akAtt & ~dkAtt;
    const bool kGuards  = akMoves & attackSquares<KING>(ar, 0);
    const bool kHitsR   = (akMoves & attackSquares<KING>(ab, 0))
                      and not ((dkAtt | attackSquares<KING>(ar, 0)) & abB);

    if (not (kRetakes or rRetakes or rBlocks or kGuards or kHitsR))
      return false;
  }

  // RxB+ and a second check win the rook. The rook takes the bishop with check,
  // and only the attacking king can answer it. The attacking rook stands on the
  // line next to the check, so wherever the king steps it either shares a line
  // with its rook (the next check skewers them) or leaves the rook to the
  // defending king.
  if (rxbChk)
  {
    const bool sameRank = (ab >> 3) == (ak >> 3);
    const bool arNext   = sameRank ? std::abs((ar >> 3) - (ak >> 3)) == 1
                                   : std::abs((ar  & 7) - (ak  & 7)) == 1;

    const bool kTakes   = (akAtt & abB) and not (dkAtt & abB);
    const bool rAnswers = arLift & (abB | (drOnB & attackSquares<ROOK>(ak, drLift)));   // takes or blocks

    // King steps that keep the rook: off its lines, and still guarding it when
    // the defending king hits it.
    const Bitboard akMoves = akAtt & ~(arB | abB | dkAtt | attackSquares<ROOK>(ab, drLift ^ akB));
    const Bitboard keepsR  = (dkAtt & arB) ? attackSquares<KING>(ar, 0) : AllSquares;
    const bool kSafe = akMoves & ~attackSquares<ROOK>(ar, 0) & keepsR;

    if (arNext and not (kTakes or rAnswers or kSafe))
      return false;
  }

  // Quiet RxB into a mating net: the attacking king is in its corner and the
  // defending king, a knight's jump away, takes two of its three squares.
  if (defToMove and (akB & CornerSquares)
      and (attackSquares<KNIGHT>(ak, 0) & dkB) and (drAtt & abB)
      and not (arLift & dkB))
    return false;

  // The defending rook can take the attacking rook. `abLift & dkB` means the
  // capture is illegal: the bishop gives check, or it pins the rook. Positions
  // where the defending king can take the rook itself (`atkCover` doesn't reach
  // it) are left out.
  if (defToMove and rHit and not (abLift & dkB) and not (dkAtt & arB & ~atkCover))
  {
    // The attacker takes back with the bishop, or with the king when the
    // defending king doesn't guard the square: KB vs K.
    if ((abLift & arB) or ((akAtt & arB) and not (dkAtt & arB)))
      return true;

    // No recapture: KR vs KB with the attacker to move. Ask the 4-man recognizer.
    if (isTheoreticalDraw(pos.play(dr, ar)))
      return true;
  }

  // The defending rook takes the bishop: KR vs KR with the attacker to move. Ask
  // the 4-man recognizer (it sees a recapture as an immediate win). Not when the
  // defender may have better: the rooks hit each other, its king hits the
  // attacking rook, or its king can take the bishop instead and that isn't a
  // known draw. Nor with the attacking king in a corner two squares from the
  // defending king (a mating net). `arLift & dkB` means the capture is illegal.
  if (defToMove and (drAtt & abB) and not (arLift & dkB)
      and not rHit and not (dkAtt & arB) and not ((akB & CornerSquares) and kingsD == 2)
      and not ((dkAtt & abB & ~(akAtt | arAtt)) and not isTheoreticalDraw(pos.play(dk, ab)))
      and isTheoreticalDraw(pos.play(dr, ab)))
    return true;

  // The defending king stands off the bishop's colour, out of check and far
  // enough from the attacking king (further when it is on the edge). The
  // defender holds unless it can win a loose piece (those can be defender
  // wins) or its rook is trapped.
  constexpr int minKingsD[4] = { 5, 3, 4, 3 };   // by dkEdge
  if (defToMove and not chk and dkOffB and kingsD >= minKingsD[dkEdge]
      and aHang == 0 and drSafe >= 2)
    return true;

  // The defender to move holds when it is out of check, its king is more than two
  // squares from the attacking king and not next to the attacking rook, the rooks
  // don't hit each other, and the bishop's lines (seen through the defender's
  // men) don't reach both its king and its rook. The exceptions are tactics and
  // mating nets.
  if (defToMove and
      not chk and
      not rHit and
      (kingsD > 2) and
      (chebyshevDistance(dk, ar) > 1)
  ) {
    // The bishop's lines, seen through the defender's men, reach both its king
    // and its rook.
    const Bitboard bishopMask = attackSquares<BISHOP>(ab, akB | arB);
    if ((bishopMask & dkB) and (bishopMask & drB))
      return false;

    // RxB+ skewers the attacking king and rook.
    if ((drAtt & abB) and xrays<ROOK>(ab, drLift, akB, arB))
      return false;

    // The defending king stands between its rook and the attacking king: a
    // discovered check.
    if (xrays<ROOK>(dr, occ, dkB, akB))
      return false;

    // The attacking king is on the edge three squares away, and in a corner or
    // the defending king can take the bishop: the defender mates.
    if (kingsD == 3 and akEdge == 0 and ((akB & CornerSquares) or (dkAtt & abB)))
      return false;

    // The bishop's line to the defending king runs through the attacking rook
    // (and maybe the defending rook): a discovered check is coming.
    if (not (attackSquares<BISHOP>(ab, arB) & dkB)
        and (attackSquares<BISHOP>(ab, drLift ^ arB) & dkB))
      return false;

    // The defending rook is nearly trapped.
    if (drSafe < 3)
      return false;

    // The defending king on the edge, close to the attacking king: mating nets.
    if (dkEdge == 0 and kingsD <= 4)
    {
      // Safe squares off the edge, how many lines the attacking king stands out
      // from the edge, and how many files aside.
      const int  dkIn     = popCount(dkAtt & ~drB & ~atkCover & ~(Rank18 | FileAH));
      const bool rankEdge = (dk >> 3) == 0 or (dk >> 3) == 7;
      const int  out      = rankEdge ? std::abs((ak >> 3) - (dk >> 3)) : std::abs((ak & 7) - (dk & 7));
      const int  aside    = rankEdge ? std::abs((ak & 7) - (dk & 7)) : std::abs((ak >> 3) - (dk >> 3));
      const bool nextToC  = dkAtt & CornerSquares;

      // Three apart: the king is cut off, the attacking king stands in front of
      // it, or it is next to a corner with the attacking king two lines out.
      if (kingsD == 3 and (dkIn <= 1 or (out == 3 and aside <= 2) or (out == 2 and nextToC)))
        return false;

      // Four apart: the king is cut off or in a corner.
      if (kingsD == 4 and (dkIn == 0 or (dkB & CornerSquares)))
        return false;
    }

    return true;
  }

  // With the attacker to move, the defender holds when the attacker is out of
  // check, the defending king is off the edge and more than two squares from the
  // attacking king, and no one-move tactic wins the rook (taking it outright was
  // ruled out above). The exceptions are those tactics and two mating nets.
  if (not defToMove and not chk and dkEdge >= 1 and kingsD >= 3)
  {
    const Bitboard dMen = dkB | drB;

    // Batteries: the bishop sees the defending king or rook through its own
    // rook, or the rook through its own bishop.
    if (xrays<BISHOP>(ab, occ, arB, dMen) or xrays<ROOK>(ar, occ, abB, dMen))
      return false;

    // The attacking king blocks its bishop's or rook's line to the defending king
    // or rook (the other attacking piece may block it too): a king move
    // discovers the attack.
    if (xrays<BISHOP>(ab, occ ^ arB, akB, dMen) or xrays<ROOK>(ar, occ ^ abB, akB, dMen))
      return false;

    // A bishop move checks the king and hits the rook (a fork, or a skewer
    // through the king), or pins the rook to its king. The attacking rook may
    // stand in the line: it steps away next move.
    for (Bitboard t = abAtt & ~occ; t != 0; t &= t - 1)
    {
      const Square   s = squareNo(t & -t);
      const Bitboard o = (occ ^ abB) | (t & -t);
      const Bitboard x = o ^ arB;
      if ((attackSquares<BISHOP>(s, o) & dkB) and (attackSquares<BISHOP>(s, x ^ dkB) & drB))
        return false;
      if (xrays<BISHOP>(s, x, drB, dkB))
        return false;
    }

    // The rook skewers the king and the rook.
    if (rookSkewers<BISHOP>(pos, ar, dk, occ))
      return false;

    // The bishop stands in the way of a skewer: with it lifted off the board, a
    // rook move checks the king with the rook behind. A bishop move with tempo
    // clears the line (8/8/8/8/k7/r7/1b1K3R/8 b, Bc3+ and Ra2+).
    for (Bitboard t = attackSquares<ROOK>(ar, occ ^ abB) & ~occ; t != 0; t &= t - 1)
    {
      const Bitboard sB = t & -t;
      const Square   s  = squareNo(sB);
      const Bitboard o  = (occ ^ arB ^ abB) | sB;
      const bool lifted = xrays<ROOK>(s, o, dkB, drB);
      const bool inLine = (arAtt & sB) and xrays<ROOK>(s, o | abB, dkB, drB);
      if (lifted and not inLine)
        return false;
    }

    // Mating net: the defending king next to a corner (b2, g2, b7, g7), three
    // squares from the attacking king.
    if (kingsD == 3 and (dkAtt & CornerSquares))
      return false;

    // The rook hits the bishop, and the attacking king can step next to the rook:
    // next to the bishop too, guarding it, or with the defending king on the
    // rook's line, open to a skewer (8/8/8/8/8/r4b2/3K1R2/7k b, Kg1 traps the
    // rook).
    const Bitboard akNextToR = akAtt & attackSquares<KING>(dr, 0) & ~(occ | dkAtt | drAtt);
    if ((drAtt & abB)
        and ((akNextToR & attackSquares<KING>(ab, 0)) or (akNextToR and (attackSquares<ROOK>(dk, 0) & drB))))
      return false;

    return true;
  }

  return false;
}

// KRN vs KR. Checked against the 5-man oracle (`egvalidate pieces RNr oracle`):
// it claims 204,895,170 of the 494,956,894 draws (41%) with no false draw, in
// both colourings. With the defender to move, the rook takes the rook or the
// knight into a 4-man draw, or the position is quiet. With the attacker to
// move, all five men stand on one line, or the defending king is far from the
// knight and the attacking king. Exclusions before each claim rule out mates
// and tactics that decide the game. KRRKN and KRRNK fall through to search.
//
// Notation: d = defender (the lone rook's side), a = attacker (rook + knight);
// k/r/n = king/rook/knight, so `dr` is the defending rook's square and `drB` its
// bitboard.
template <>
bool
Endgame<Endgames::KRNKR>(const EgBoard& pos)
{
  const Color atk = pos.count<WHITE, KNIGHT>() ? WHITE : BLACK;
  const Color def = ~atk;

  if (popCount(pos.getPiece(atk, ROOK)) != 1)
    return false;

  const Bitboard akB = pos.getPiece(atk, KING  );
  const Bitboard arB = pos.getPiece(atk, ROOK  );
  const Bitboard anB = pos.getPiece(atk, KNIGHT);
  const Bitboard dkB = pos.getPiece(def, KING  );
  const Bitboard drB = pos.getPiece(def, ROOK  );

  const Square ak = squareNo(akB), ar = squareNo(arB), an = squareNo(anB);
  const Square dk = squareNo(dkB), dr = squareNo(drB);
  const Bitboard occ = pos.all();

  const Bitboard akAtt = attackSquares<KING  >(ak, 0);
  const Bitboard arAtt = attackSquares<ROOK  >(ar, occ);
  const Bitboard anAtt = attackSquares<KNIGHT>(an, 0);
  const Bitboard dkAtt = attackSquares<KING  >(dk, 0);
  const Bitboard drAtt = attackSquares<ROOK  >(dr, occ);

  // Attacker to move. The tests below rule out the rook captures, knight forks
  // and skewers that win outright, before the claims.
  if (pos.color == atk)
  {
    // The knight, the rook or the king takes the rook.
    const bool nxr = (anAtt & drB)
      and loneKingLoses(dk, akAtt | attackSquares<KNIGHT>(dr, 0) | attackSquares<ROOK>(ar, occ ^ anB ^ dkB), arB);
    const bool rxr = (arAtt & drB)
      and loneKingLoses(dk, akAtt | anAtt | attackSquares<ROOK>(dr, occ ^ arB ^ dkB), drB);
    const bool kxr = (akAtt & drB) and not (dkAtt & drB)
      and loneKingLoses(dk, attackSquares<KING>(dr, 0) | anAtt | attackSquares<ROOK>(ar, occ ^ akB ^ dkB), arB);
    if (nxr or rxr or kxr)
      return false;

    // The knight forks the king and the rook, or the rook skewers them.
    if (knightForks(pos, dk, ar, occ) or rookSkewers<KNIGHT>(pos, ar, dk, occ))
      return false;

    // The rook checks from s and every king step walks into a skewer or a
    // knight fork. The defending rook must not take on s, block, or leave the
    // attacking king in check, and the king must not take on s.
    for (Bitboard t = arAtt & ~occ & attackSquares<ROOK>(dk, occ); t; t &= t - 1)
    {
      const Bitboard sB   = t & -t;
      const Square   s    = squareNo(sB);
      const Bitboard occS = occ ^ arB ^ sB;
      const Bitboard line = attackSquares<ROOK>(s, occS) & attackSquares<ROOK>(dk, occS);
      if (attackSquares<ROOK>(dr, occS) & (akB | sB | line))
        continue;
      const Bitboard esc = dkAtt & ~drB & ~(akAtt | anAtt | attackSquares<ROOK>(s, occS ^ dkB));
      bool lost = not (esc & sB);
      for (Bitboard u = esc; u != 0 and lost; u &= u - 1)
      {
        const Square   e    = squareNo(u & -u);
        const Bitboard occE = occS ^ dkB ^ (u & -u);
        lost = rookSkewers<KNIGHT>(pos, s, e, occE) or knightForks(pos, e, s, occE);
      }
      if (lost)
        return false;
    }

    const auto mIndex = msbIndex(occ);
    const auto lIndex = lsbIndex(occ);
    const auto pieceOnSameRank = (abs(mIndex - lIndex) <= 8) and ((lIndex >> 3) == (mIndex >> 3));
    // Squares are numbered rank by rank, so lsb and msb on one rank put every
    // piece on it. A file has no such ordering: mask lsb's file instead.
    const bool pieceOnSameFile = not (occ & ~(FileA << (lIndex & 7)));
    const bool rHit = drAtt & arB;

    if (pieceOnSameRank or pieceOnSameFile)
      return rHit;

    // The defending king is far from the knight and the attacking king, so no
    // fork or mating net is in reach. Distances are in king steps. A piece on
    // dk's rank or file can uncover a check or check with tempo, so it counts
    // as far only at 7, and the knight also counts as far at Manhattan
    // distance 8. A king in a corner needs one step more, and the attacking
    // king must not be in check (its rook can hang).
    const Bitboard dkLines = attackSquares<ROOK>(dk, 0);
    const int      kD      = chebyshevDistance(dk, ak);
    const int      nD      = chebyshevDistance(dk, an);
    const bool     akFar   = kD == 7 or (kD >= ((dkB & CornerSquares) ? 5 : 4) and not (dkLines & akB));
    const bool     anFar   = nD == 7 or ((nD >= 5 or manhattanDistance(dk, an) >= 8) and not (dkLines & anB));
    return not (drAtt & akB) and akFar and anFar;
  }

  // Defender to move.
  const int kingsD = chebyshevDistance(ak, dk);
  const int dkEdge = edgeDistance(dk);

  // The defending king is in check.
  const bool chk = (arAtt | anAtt) & dkB;

  // The rooks hit each other. A rook trade leaves KN vs K.
  const bool rHit = drAtt & arB;

  // Squares the attacker covers. The rook sees through the defending king (it
  // cannot step back along the line it is checked on).
  const Bitboard atkCover = akAtt | anAtt | attackSquares<ROOK>(ar, occ ^ dkB);

  // The attacking rook's lines with the defending rook lifted off its square.
  const Bitboard drLift = occ ^ drB;
  const Bitboard arLift = attackSquares<ROOK>(ar, drLift);

  if (not chk)
  {
    // The defending rook mates in one, or threatens a back-rank mate.
    if (rookMatesInOne<KNIGHT>(pos) or backRankThreat(pos))
      return false;

    // The attacking king in a corner is open to a mating attack. With the
    // defending king two away, it already covers the corner king's flights.
    // Three away, with the attacking rook on one of its king's lines and the
    // defending king off them: the defending king steps a knight's jump from
    // the corner and the rook threatens mate on the other line
    // (8/8/8/8/2r5/8/3k1N2/K4R2 b, Kc2).
    if (akB & CornerSquares)
    {
      const Bitboard akLines = attackSquares<ROOK>(ak, 0);

      if (kingsD == 2)
        return false;
      if (kingsD == 3 and (akLines & arB) and not (akLines & dkB)
          and (dkAtt & attackSquares<KNIGHT>(ak, 0) & ~atkCover))
        return false;
    }
  }

  // RxN gives check and doesn't reach a known KR vs KR draw: it may win for the
  // defender.
  const bool rxnWins = (drAtt & anB) and (attackSquares<ROOK>(an, drLift) & akB)
                   and not (arLift & dkB) and not isTheoreticalDraw(pos.play(dr, an));

  // The defending rook takes the attacking rook. Illegal when the knight
  // gives check.
  if (rHit and not (anAtt & dkB) and not rxnWins)
  {
    // The rook is guarded, so RxR is answered by a recapture into KN vs K.
    if ((anAtt & arB) or ((akAtt & arB) and not (dkAtt & arB)))
      return true;

    // Unguarded: RxR leaves KR vs KN.
    if (not (dkAtt & arB) and isTheoreticalDraw(pos.play(dr, ar)))
      return true;
  }

  // The defending king stands between its rook and the attacking king.
  const bool discChk = xrays<ROOK>(dr, occ, dkB, akB);

  // A defender rook move checks the attacking king and hits the attacking rook
  // behind it or beside it (seen through the king), from a square nothing can
  // take the rook on: not the knight or the rook, and the king only when the
  // defending king doesn't guard it.
  bool dSkew = false;
  for (Bitboard t = drAtt & ~occ & ~anAtt & ~arLift & (~akAtt | dkAtt); t != 0 and not dSkew; t &= t - 1)
  {
    const Square   s = squareNo(t & -t);
    const Bitboard o = drLift | (t & -t);
    dSkew = xrays<ROOK>(s, o, akB, arB);
  }

  // The attacking king is on an edge and its rook on the next line in: a rook
  // check along the edge drives the king onto its rook's line. Defender rook
  // moves that give that check.
  Bitboard edgeLine = 0;
  if (((ak >> 3) == 0 and (ar >> 3) == 1) or ((ak >> 3) == 7 and (ar >> 3) == 6))
    edgeLine |= Rank1 << (8 * (ak >> 3));
  if (((ak & 7) == 0 and (ar & 7) == 1) or ((ak & 7) == 7 and (ar & 7) == 6))
    edgeLine |= FileA << (ak & 7);
  const bool edgeSkew = drAtt & ~occ & edgeLine & attackSquares<ROOK>(ak, drLift);

  // Two checks win the rook: the check along the edge can't be taken or
  // blocked, every flight is on the rook's line and too far from it to guard
  // it, and from each flight a second check from a safe square skewers king
  // and rook. The knight doesn't guard the rook.
  const auto twoChecks = [&] () {
    if (anAtt & arB)
      return false;
    for (Bitboard t = drAtt & ~occ & edgeLine; t != 0; t &= t - 1)
    {
      const Bitboard s1B = t & -t;
      const Square   s1  = squareNo(s1B);
      const Bitboard o1  = drLift | s1B;
      const Bitboard r1  = attackSquares<ROOK>(s1, o1);
      const Bitboard ar1 = attackSquares<ROOK>(ar, o1);
      if (not (r1 & akB) or ((anAtt | ar1) & (s1B | (r1 & attackSquares<ROOK>(ak, o1))))
          or ((akAtt & s1B) and not (dkAtt & s1B)))
        continue;

      const Bitboard flights = akAtt & ~(arB | anB | dkAtt | s1B) & ~attackSquares<ROOK>(s1, o1 ^ akB);
      bool wins = flights != 0;
      for (Bitboard f = flights; f != 0 and wins; f &= f - 1)
      {
        const Bitboard fB = f & -f;
        const Square   fs = squareNo(fB);
        const Bitboard o2 = (o1 ^ akB) | fB;
        bool skew = false;
        if (chebyshevDistance(fs, ar) >= 3)
          for (Bitboard u = attackSquares<ROOK>(s1, o2) & ~o2; u != 0 and not skew; u &= u - 1)
          {
            const Bitboard s2B = u & -u;
            const Square   s2  = squareNo(s2B);
            const Bitboard o3  = (o2 ^ s1B) | s2B;
            const Bitboard r2  = attackSquares<ROOK>(s2, o3);
            skew = xrays<ROOK>(s2, o3, fB, arB)
                   and not ((anAtt | attackSquares<ROOK>(ar, o3)) & (s2B | (r2 & attackSquares<ROOK>(fs, o3))))
                   and not ((attackSquares<KING>(fs, 0) & s2B) and not (dkAtt & s2B));
          }
        wins = skew;
      }
      if (wins)
        return true;
    }
    return false;
  };
  const bool edgeTwo = edgeSkew and twoChecks();

  // The defending rook takes the knight: KR vs KR with the attacker to move.
  if ((drAtt & anB) and not (arLift & dkB)
      and not rHit and not (dkAtt & arB) and not ((akB & CornerSquares) and kingsD <= 3)
      and not dSkew and not discChk and not edgeTwo
      and isTheoreticalDraw(pos.play(dr, an)))
    return true;

  // Quiet: no check, no rook trade, and the attacking rook out of the
  // defending king's reach.
  if (chk or rHit or (dkAtt & arB))
    return false;

  // Moves of the defending king to squares the attacker does not cover.
  const int dkSafe = popCount(dkAtt & ~drB & ~atkCover);

  // Defending rook moves to squares where it is not lost: the knight does not
  // hit it, and the attacking king and rook either do not hit it or the
  // defending king guards it (RxR KxR is KN vs K).
  const Bitboard drSafeSq = ~anAtt & (~(akAtt | arLift) | dkAtt);
  const int drSafe = popCount(drAtt & ~dkB & drSafeSq);

  if (dSkew or dkSafe == 0 or discChk or drSafe < 3 or edgeTwo or rxnWins)
    return false;

  // The defending king on the edge with few safe squares off it: a mating net.
  const int dkIn = popCount(dkAtt & ~drB & ~atkCover & ~(Rank18 | FileAH));
  if (dkEdge == 0 and ((kingsD == 3 and dkIn <= 1) or (kingsD <= 5 and dkIn == 0)))
    return false;

  // Knight tactics against the rook. The battery: the knight stands between
  // the attacking rook and the defending rook. RxN fails when the rook guards
  // the knight.
  const bool drHang  = ((akAtt | arAtt | anAtt) & drB) and not (dkAtt & drB);
  const int  drND    = chebyshevDistance(dr, an);
  const bool battery = xrays<ROOK>(ar, occ, anB, drB);
  if (drHang and drND == 1 and (arAtt & anB))
    return false;
  if (battery and (dkB & CornerSquares))
    return false;
  if (dkEdge == 0 and drHang and drND == 2 and (kingsD == 3 or dkSafe == 1))
    return false;
  if (dkEdge == 0 and kingsD == 3 and drND == 1 and (arAtt & anB))
    return false;

  // The kings two apart. Not with the defending king on the edge (a mating
  // net), nor facing the attacking king on its edge (the defender's back-rank
  // attack). A knight's jump apart, a loose rook or a discovered check from
  // the knight loses for the defender.
  if (kingsD == 2)
  {
    const int fileD = std::abs((ak & 7) - (dk & 7));
    const int rankD = std::abs((ak >> 3) - (dk >> 3));
    const bool nDisc = xrays<ROOK>(ar, occ, anB, dkB);

    if (dkEdge == 0)
      return false;
    if ((fileD == 0 or rankD == 0) and edgeDistance(ak) == 0)
      return false;
    if (fileD != rankD and fileD != 0 and rankD != 0 and (drHang or nDisc))
      return false;
  }

  return true;
}

static bool
isTheoreticalDraw(const EgBoard& pos)
{
  int pieceCount = pos.count<ALL>();
  if (pieceCount > 3)
    return false;

  if (pieceCount == 0)
    return true;

  if (pieceCount == 1)
  {
    if (isEndgame<Endgames::KPK>(pos))
      return Endgame<Endgames::KPK>(pos);

    if (isEndgame<Endgames::KBK>(pos) or isEndgame<Endgames::KNK>(pos))
      return true;

    return false;
  }

  if (pieceCount == 2)
  {
    if (isEndgame<Endgames::KNNK>(pos))
      return Endgame<Endgames::KNNK>(pos);

    if (isEndgame<Endgames::KPBK>(pos))
      return Endgame<Endgames::KPBK>(pos);

    if (isEndgame<Endgames::KPQK>(pos))
      return Endgame<Endgames::KPQK>(pos);

    if (isEndgame<Endgames::KBBK>(pos))
      return Endgame<Endgames::KBBK>(pos);

    if (isEndgame<Endgames::KBNK>(pos))
      return Endgame<Endgames::KBNK>(pos);

    if (isEndgame<Endgames::KRBK>(pos))
      return Endgame<Endgames::KRBK>(pos);

    if (isEndgame<Endgames::KRNK>(pos))
      return Endgame<Endgames::KRNK>(pos);

    if (isEndgame<Endgames::KRKR>(pos))
      return Endgame<Endgames::KRKR>(pos);

    if (isEndgame<Endgames::KPNK>(pos))
      return Endgame<Endgames::KPNK>(pos);

    if (isEndgame<Endgames::KPRK>(pos))
      return Endgame<Endgames::KPRK>(pos);
  }

  if (pieceCount == 3)
  {
    if (isEndgame<Endgames::KBNKN>(pos))
      return Endgame<Endgames::KBNKN>(pos);

    if (isEndgame<Endgames::KRBKR>(pos))
      return Endgame<Endgames::KRBKR>(pos);

    if (isEndgame<Endgames::KRNKR>(pos))
      return Endgame<Endgames::KRNKR>(pos);
  }

  return false;
}

bool
isTheoreticalDraw(const ChessBoard& pos)
{
  // Search calls this at every node; leave before building the EgBoard.
  if (pos.count<ALL>() > 3)
    return false;

  return isTheoreticalDraw(EgBoard(pos));
}
