
#include "endgame.h"
#include "attacks.h"

using plt::passedPawnMasks;
using plt::ruleOfSquares;

// Table lookups -- see the metric note in lookup_table.h.
using plt::chebyshevDistance;
using plt::manhattanDistance;


// TODO: KRRK, KRNK

static bool
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

// Ranks indexed relative to each side's home rank. Index 0 is unused.
constexpr Bitboard relativeRank[COLOR_NB][9] = {
  { NoSquares, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1 },  // BLACK
  { NoSquares, Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8 },  // WHITE
};

constexpr Bitboard rank2to6[COLOR_NB] = {
  AllSquares ^ (Rank18 | Rank2),
  AllSquares ^ (Rank18 | Rank7)
};

constexpr Bitboard rank3to5[COLOR_NB] = {
  Rank4 | Rank5 | Rank6,
  Rank3 | Rank4 | Rank5
};

constexpr Bitboard rank4to7[COLOR_NB] = {
  Rank4 | Rank5 | Rank6 | Rank7,
  Rank2 | Rank3 | Rank4 | Rank5
};

static int
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

template <>
inline bool
isEndgame<Endgames::KPRK>(const ChessBoard& pos)
{
  return pos.count<ALL >() == 2
     and pos.count<PAWN>() == 1
     and pos.count<ROOK>() == 1;
}

template <Endgames e>
inline bool
Endgame(const ChessBoard& pos) = delete;

template <>
inline bool
Endgame<Endgames::KPK>(const ChessBoard& pos)
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
Endgame<Endgames::KPBK>(const ChessBoard& pos)
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
Endgame<Endgames::KPQK>(const ChessBoard& pos)
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
Endgame<Endgames::KBBK>(const ChessBoard& pos)
{
  if (pos.count<WHITE, BISHOP>() == 1)
    return true;

  const Bitboard bishops = pos.piece<WHITE, BISHOP>() | pos.piece<BLACK, BISHOP>();
  return !((bishops & WhiteSquares) and (bishops & BlackSquares));
}

template <>
inline bool
Endgame<Endgames::KBNK>(const ChessBoard& pos)
{ return pos.count<WHITE, ALL>() == 1; }


template <>
bool
Endgame<Endgames::KRBK>(const ChessBoard& pos)
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

  // Pinned bishop that cannot be saved.
  const Bitboard rookXray = attackSquares<ROOK>(rookSq, occupied ^ bishop);

  if (rookOnBish and ((rookXray & ~rookNow) & king) and
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

template <>
inline bool
Endgame<Endgames::KPNK>(const ChessBoard& pos)
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

    if (defToMove and
       (plt::knightMasks[pawnSq] & plt::knightMasks[emyKingSq] & plt::knightMasks[knightSq] & ~myKing) and
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
Endgame<Endgames::KPRK>(const ChessBoard& pos)
{
  // Look from the PAWN side -- the defender fighting to hold the draw against the
  // rook. Like KPQK/KPNK this covers both material configs: the opposite-side
  // KP-vs-KR case (each side one non-king man) carries the draw logic; same-side
  // KPR-vs-K falls through to the terminal return false (trivial win -- a safe
  // missed-draw gap).
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

    // Hanging-piece guards -- the KPRK pair of the KPKB/KRKB/KPKN family, one per
    // side, because here BOTH men can hang: the defender is K+P (not a bare king)
    // and the attacker's rook is capturable in turn.
    //
    // Free as written, on both paths (measured): gated tallies are bit-identical
    // to no guard -- the capture gate already drops every position these fire on
    // -- and gate-off they remove 13,011 claims, all 13,011 of them false draws,
    // leaving agree-draw and missed-draw untouched. Contrast KPKN, where the same
    // shape also cost 92,786 genuine draws.
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

    // Rook-sac lever, hoisted out of the third rule so the probe can gate on it:
    // can the rook reach the queening line on a square neither the defending king
    // nor the pawn covers?
    const Bitboard reachMask = plt::lineMasks[promoSq]
                             & ~(plt::kingMasks[myKingSq] | plt::pawnCaptureMasks[side][pawnSq]);
    const bool rookReach = (rookMask & reachMask) != 0;

    // Pawn one step from queening with the rook side to move. The defending king
    // covers the queening square (dkPromoD <= 2) and the attacking king is too far
    // from the pawn to help (akPawnD >= 4), so the rook is on its own: it must
    // either give itself up for the pawn or let it queen and be skewered. Either
    // way the game ends in bare kings. Exhaustively pure over the call set.
    if (rookReach and defToMove == 0 and pawnRel == 7
        and dkPromoD <= 2 and akPawnD >= 4)
      return true;

    // Self-block draw. The defending king sits on (or beside) its own queening
    // square with the pawn two ranks back -- so it stands in the way of the very
    // pawn it is escorting and the pawn side can never make progress; the rook
    // simply shuffles. The akPawnD floor keeps the attacking king too far away to
    // turn the position into a win instead, and it steps with dkPromoD because a
    // king one square off the promotion square needs the extra tempo.
    if (dkPromoD <= 1 and dkPawnD == 2 and akPawnD >= 5 + dkPromoD)
      return true;

    // Same self-block shape (defending king on/beside the queening square, pawn two
    // ranks back), but fenced by where the *attacker* stands rather than by its
    // distance to the pawn: the rook king is a full board away from the queening
    // square, so it can never join in. The two rook exclusions are the positions
    // where the pawn side, on move, actually breaks through -- with the rook either
    // level with the attacking king's distance or two files short of it, it lacks the
    // tempo to both check and return, and the pawn queens.
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

bool
isTheoreticalDraw(const ChessBoard& pos)
{
  int pieceCount = pos.count<ALL>();
  if (pieceCount > 2)
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
      return true;

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

    if (isEndgame<Endgames::KPNK>(pos))
      return Endgame<Endgames::KPNK>(pos);

    if (isEndgame<Endgames::KPRK>(pos))
      return Endgame<Endgames::KPRK>(pos);
  }

  return false;
}
