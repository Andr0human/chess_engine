
#include "endgame.h"
#include "attacks.h"
#include "bucket_probe.h"

using plt::passedPawnMasks;
using plt::ruleOfSquares;


// TODO: KRRK, KRNK

static int
chebyshevDistance(Square s1, Square s2)
{
  int rank1 = s1 >> 3, file1 = s1 & 7;
  int rank2 = s2 >> 3, file2 = s2 & 7;
  return std::max(abs(rank1 - rank2), abs(file1 - file2));
}

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

// Color-relative ranks: relativeRank[color][r] is the r-th rank (1-8) counting
// from color's own back rank, so the BLACK row mirrors to absolute rank 9-r.
// Ranks are 1-based; index 0 is a NoSquares placeholder so call sites read naturally.
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

  int ruleOfSquareIndex = pawnSq;
  if (!sideAdvantage)
    ruleOfSquareIndex -= 8 * incFactor;

  if ((pawn & relativeRank[side][2]) and (myKing & ~plt::pawnMasks[side][pawnSq]))
    ruleOfSquareIndex += 8 * incFactor;

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

  // if ((pawnSq + 8 * incFactor == emyKingSq) and
  //     (pawnSq - 8 * incFactor ==  myKingSq) and
  //     (!sideAdvantage or (!(emyKing & Rank18) and sideAdvantage))
  // ) return true;

  // if ((pawnSq + 8 * incFactor == emyKingSq) and
  //     (pawn & (AllSquares ^ FileAH)) and
  //     ((pawnSq - 7 * incFactor == myKingSq) or (pawnSq - 9 * incFactor == myKingSq)) and
  //     (sideAdvantage or (!(emyKing & Rank18) and !sideAdvantage))
  // ) return true;

  // if ((pawn & (AllSquares ^ FileAH)) and
  //     ((pawnSq + 1 == myKingSq) or (pawnSq - 1 == myKingSq)) and
  //     ((pawnSq + 16 * incFactor == emyKingSq) and !sideAdvantage)
  // ) return true;

  // if ((pawnSq +  8 * incFactor ==  myKingSq) and
  //     (pawnSq + 24 * incFactor == emyKingSq) and
  //     (sideAdvantage and !(emyKing & Rank18))
  // ) return true;

  // Defender-king blockade draw: the defending king sits exactly two squares
  // directly in front of the pawn (same file) and it is the attacker's move.
  //   - attacker king one rank behind the pawn (d3/e3/f3 for a pawn on e4):
  //     always a draw (defender holds the opposition / stalemate resource).
  //   - attacker king beside the pawn (d4/f4): a draw unless the defender is
  //     forced onto a non-rook-file promotion square, which is the only winning
  //     break; on the a/h file that square is still the rook-pawn corner draw.
  // Complements the blocks above which cover this geometry with the defender to move.
  // if (sideAdvantage and (emyKingSq == pawnSq + 16 * incFactor))
  // {
  //   const int pawnF    = pawnSq   & 7;
  //   const int myKingF  = myKingSq & 7;
  //   const int fileDist = abs(myKingF - pawnF);
  //   const bool behindKing = (myKingR == pawnR - incFactor) and (fileDist <= 1);
  //   const bool besideKing = (myKingR == pawnR)             and (fileDist == 1);
  //   if (behindKing or (besideKing and (!(emyKing & Rank18) or (emyKing & FileAH))))
  //     return true;
  // }

  return false;
}

template <>
inline bool
Endgame<Endgames::KPBK>(const ChessBoard& pos)
{
  // Look from side who has bishop
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

  // Wrong-bishop rook-pawn draw: checked before the non-draw block below so a
  // corner-held draw isn't pre-empted by the pawn being bishop/king-defended or
  // ahead of the enemy king. With the wrong bishop the king can't be evicted.
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

  // If pawn is attacked by enemy king and our king cannot support the pawn
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
  const int incFactor = 2 * side - 1;

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
    const Square promoSq = pawnSq + 8 * incFactor;

    const bool pawnOnRank7 = !!(pawn & relativeRank[side][7]);
    const bool kingNotOnPromoSq = !(myKing & (1ULL << promoSq));
    const int  distanceBtwKings = chebyshevDistance(kingSq, emyKingSq);

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
    // Condition-1 pre-check needed
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

      if ((myKingF == pawnF) or (abs(myKingF - pawnF) == 1 and (myKing & FileAH)))
      {
        Bitboard mask = side == WHITE ? plt::downMasks[kingSq] : plt::upMasks[kingSq];
        mask &= ~kingMask;
        if (queenMask & mask)
          return false;
      }

      // King and promo square share the a1-h8 diagonal (file - rank constant).
      // A bare `% 9` index test wrongly fires on file-wrapped differences that
      // happen to be multiples of 9, which breaks colour symmetry (the index
      // delta is not preserved under a vertical mirror).
      if ((myKingF - myKingR) == (pawnF - (promoSq >> 3)))
      {
        Bitboard mask = side == WHITE ? plt::downLeftMasks[kingSq] : plt::upRightMasks[kingSq];
        mask &= ~kingMask;
        if (queenMask & mask)
          return false;
      }

      // Same for the a8-h1 anti-diagonal (file + rank constant); `% 7` had the
      // identical file-wrap false-positive bug.
      if ((myKingF + myKingR) == (pawnF + (promoSq >> 3)))
      {
        Bitboard mask = side == WHITE ? plt::downRightMasks[kingSq] : plt::upLeftMasks[kingSq];
        mask &= ~kingMask;
        if (queenMask & mask)
          return false;
      }

      if ((myKingR == (promoSq >> 3)))
      {
        Bitboard mask = (pawnF > myKingF) ? plt::leftMasks[kingSq] : plt::rightMasks[kingSq];
        mask &= ~kingMask;
        if (queenMask & mask)
          return false;
      }

      if ((kingMask & ~(queenMask | emyKingMask)) and
          (distanceBtwKings > 2 + int(!!(myKing & EdgeSquares))) and
         !((emyKingMask & queen) and (queen & CornerSquares)) and
         !((queenMask | emyKingMask) & myKing)
      ) return true;
    }

    // Condition-1 pre-check needed
    if (sideAdvantage and
        (myKing & (relativeRank[side][8] & FileAH)) and
        pawnOnRank7 and
        (abs(pawnF - myKingF) == 2) and
        (abs(pawnF - emyKingF)   > 2)
    ) return true;      // 2729

    if (!sideAdvantage and
       (myKing & CornerSquares) and
       (kingMask & pawn) and
       ((kingMask ^ (kingMask & (queenMask | pawn))) == 0) and
       (distanceBtwKings > (chebyshevDistance(kingSq, queenSq))) and
       (distanceBtwKings > 4)
    ) return true;

    if (pawnOnRank7 and
        sideAdvantage and
       (myKing & (relativeRank[side][7] | relativeRank[side][8])) and
       (chebyshevDistance(pawnSq, emyKingSq) > 2) and
       (((pawn & FileC) and (myKing & (FileA | FileB)))
     or ((pawn & FileF) and (myKing & (FileG | FileH))))
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


/* template <>
bool
Endgame<Endgames::KRBK>(const ChessBoard& pos)
{
  if ((pos.count<WHITE, ALL>() == 2) or (pos.count<BLACK, ALL>() == 2))
    return false;

  const Color side = pos.count<WHITE, ROOK>() ? WHITE : BLACK;
  const Color emySide = ~side;

  const Bitboard emyKing = pos.getPiece(emySide, KING  );
  const Bitboard bishop  = pos.getPiece(emySide, BISHOP);

  const Square    kingSq = squareNo(pos.getPiece(side, KING));
  const Square emyKingSq = squareNo(emyKing);
  const Square  bishopSq = squareNo(bishop);

  const int emyKingSqR = emyKingSq >> 3;
  const int emyKingSqF = emyKingSq &  7;
  const int  bishopSqR = bishopSq  >> 3;
  const int  bishopSqF = bishopSq  &  7;

  const Square rookSq = squareNo(pos.getPiece(side, ROOK));
  const bool rookHitsBishop = attackSquares<ROOK>(rookSq, 0) & bishop;

  if (!(emyKing & (Rank18 | FileAH)) and
      !rookHitsBishop and
      (chebyshevDistance(kingSq, bishopSq) > 3) and
      (abs(emyKingSqR - bishopSqR) > 1 and abs(emyKingSqF - bishopSqF) > 1)
  ) return true;

  // Center fortress (data-mined against the perfect KRKB oracle, FALSE-DRAW-free
  // over the full sweep): the defending king is off the edge with its bishop
  // guarded by that king (so the rook cannot win it) -> a held draw. The
  // king-separation bound is asymmetric: with the defender to move the kings need
  // only be >= 3 apart, but with the *attacker* to move it can win a tempo at
  // exactly 3, so it needs >= 4 (every dist-3 attacker-to-move case is a win).
  if (!(emyKing & (Rank18 | FileAH)) and
      (chebyshevDistance(emyKingSq, bishopSq) == 1) and
      (chebyshevDistance(kingSq, emyKingSq) >= 3 + int(pos.color == side)) and
      !rookHitsBishop
  ) return true;

  return false;
} */

template <>
bool
Endgame<Endgames::KRBK>(const ChessBoard& pos)
{
  if ((pos.count<WHITE, ALL>() == 2) or (pos.count<BLACK, ALL>() == 2))
    return false;

  const Color side = pos.count<WHITE, BISHOP>() ? WHITE : BLACK;
  const Color emySide = ~side;
  const int sideAdvantage = side == pos.color;

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

  const int    distBtwKings  = chebyshevDistance(kingSq, emyKingSq);

  const Bitboard bishopMask  = attackSquares<BISHOP>(bishopSq , king);
  const Bitboard rookMask    = attackSquares< ROOK >(rookSq   , emyKing);
  const Bitboard kingMask    = attackSquares< KING >(kingSq   , 0);
  const Bitboard emyKingMask = attackSquares< KING >(emyKingSq, 0);

  const Bitboard rookDiag = attackSquares<BISHOP>(rookSq, 0);

  if (sideAdvantage and (rookDiag & emyKing))
  {
    const auto mask = attackSquares<BISHOP>(emyKingSq, 0) & attackSquares<BISHOP>(rookSq, 0);
    if (((mask & bishopMask) & ~emyKingMask) and (king & ~rookMask))
      return true;
  }

  if ((rookMask & king) and (rookMask & bishop))
  {
    const Bitboard inBtwMask = attackSquares<ROOK>(rookSq, occupied) & attackSquares<ROOK>(kingSq, occupied);
    const Bitboard bishopInBtw = (bishopMask | bishop) & inBtwMask;

    if (!bishopInBtw and
       (chebyshevDistance(kingSq, bishopSq) > 2)
    ) return false;
  }

  if (kingR == bishopR)
  {
    const Bitboard inBtwMask = attackSquares<ROOK>(kingSq, 0) & attackSquares<ROOK>(bishopSq, 0);
    if (!sideAdvantage and
       (emyKingF != rookF) and
       (chebyshevDistance(kingSq, bishopSq) > 2) and
       (abs(kingF - rookF) > 1) and
       (emyKing & ~inBtwMask) and
      !(bishopMask & emyKing)
    ) return false;
  }

  if (kingF == bishopF)
  {
    const Bitboard inBtwMask = attackSquares<ROOK>(kingSq, 0) & attackSquares<ROOK>(bishopSq, 0);
    if (!sideAdvantage and
       (emyKingR != rookR) and
       (chebyshevDistance(kingSq, bishopSq) > 2) and
       (abs(kingR - rookR) > 1) and
       (emyKing & ~inBtwMask) and
      !(bishopMask & emyKing)
    ) return false;
  }

  // --------------------------------------------------------------------------------------

  if ((king & CornerSquares) and (distBtwKings == 2))
  {
    if (((kingR + kingF) & 1) == ((bishopR + bishopF) & 1))
      return false;

    if (((kingR == emyKingR and kingF == rookF) or (kingR == rookR and kingF == emyKingF)) and
       !(bishopMask & kingMask)
    ) return false;

    if (!sideAdvantage)
      return false;

    if (rookMask & king)
      return false;

    // A loose bishop offers no defensive resource: undefended by its own king
    // and not attacking the rook, it lets the enemy rook mate the cornered king
    // (the attacker's king is already only 2 squares away). Only claim the draw
    // when the bishop is actually held.
    const bool bishopGuarded  = (kingMask & bishop) != 0;
    const bool bishopHitsRook = (attackSquares<BISHOP>(bishopSq, occupied) & rook) != 0;
    if (!bishopGuarded and !bishopHitsRook)
      return false;
  }

  // Defender king cornered on a square the bishop's colour matches, attacker to
  // move, kings three apart: the mating net closes exactly as it does at
  // distance two (handled above) -- the extra king step is absorbed on the
  // attacker's move. Verified pure-decided against the oracle (~95%+ wins,
  // ~20:1 decided-to-drawn), so claiming a draw here would be a false-draw.
  if (!sideAdvantage and (king & CornerSquares) and (distBtwKings == 3) and
     (((kingR + kingF) & 1) == ((bishopR + bishopF) & 1))
  ) return false;

  // Rook forks king and bishop, defender to move, bishop stranded from its own
  // king. The rook bears on the defender king (giving check, so the defender
  // must respond) and on the bishop at the same time; with the bishop a full
  // king-move away (chebyshev >= 5) the king cannot step out of check and still
  // cover it, so the bishop drops -> decided. Verified against the oracle: this
  // slice runs ~7:1 decided-to-drawn (kills ~21k false-draws for ~3k safe missed
  // draws), climbing to ~15:1 at distance 7. Claiming a draw here is a false-draw.
  {
    const Bitboard rookRay = attackSquares<ROOK>(rookSq, occupied);
    if (sideAdvantage and (rookRay & bishop) and (rookRay & king) and
        chebyshevDistance(kingSq, bishopSq) >= 5)
      return false;
  }

  // Kings in opposition along the edge the defender is pinned to, attacker to
  // move. On a file-edge (A/H) opposition means equal ranks; on a rank-edge
  // (1/8) it means equal files. With the defender king on the edge, the kings
  // two squares apart and directly opposed, and the attacker on the move, the
  // rook drives the king back with the attacker's king holding the opposition:
  // the defender cannot hold the bishop and the square at once -> decided.
  // Verified against the oracle: this slice runs ~2.67:1 decided-to-drawn
  // (kills ~20k false-draws for ~7k safe missed draws). Claiming a draw here is
  // a false-draw.
  {
    const bool kingsAlign =
      king & FileAH ? kingR == emyKingR
                    : (king & Rank18 ? kingF == emyKingF : false);
    if ((king & EdgeSquares) and distBtwKings == 2 and not sideAdvantage and
        kingsAlign)
      return false;
  }

  // Bucket instrumentation (egvalidate only): tag this position with a feature
  // vector so the harness can group and read each bucket's oracle WDL. Emitted
  // here -- after every carve-out, just before the terminal draw verdict -- so
  // only the *leftover* positions we still claim as draws are bucketed. The
  // ones already decided by a return-false carve-out above are excluded.
  if (BucketProbe::enabled)
  {
    // Does the defender king have any legal move? A king step is legal if it
    // stays off its own bishop's square, out of the enemy king's contact zone,
    // and off the rook's rays -- the rays are recomputed with the king lifted
    // (occupied ^ king), so a square screened only by the king still counts as
    // attacked. The rook's own square is never on its ray, so capturing an
    // undefended rook is correctly counted as a legal move.
    // const Bitboard rookRayNoKing = attackSquares<ROOK>(rookSq, occupied ^ king);
    // const Bitboard kingLegalMask =
    //   kingMask & ~(bishop | emyKingMask | rookRayNoKing);
    // const bool defKingHasMove = kingLegalMask != 0;

    // Rook bearing directly on the bishop along a clear ray (real occupancy, so
    // intervening pieces block). A rook already attacking the bishop threatens to
    // win it.
    // const Bitboard rookRay = attackSquares<ROOK>(rookSq, occupied);
    // const bool rookAttacksBishop = (rookRay & bishop) != 0;

    // Colour of the corner nearest the defender king vs the bishop's square
    // colour. A defender driven toward a corner the bishop cannot cover
    // (opposite colour) is the textbook losing pattern.
    // const int  cornerR = (kingR >= 4) ? 7 : 0;
    // const int  cornerF = (kingF >= 4) ? 7 : 0;
    // const bool cornerColorMatch =
    //   (((cornerR + cornerF) & 1) == ((bishopR + bishopF) & 1));
    // const int bishopColorMatch = (((kingR + kingF) & 1) == ((bishopR + bishopF) & 1));

    const auto kingsAlign =
      king & FileAH ? kingR == emyKingR : (king & Rank18 ? kingF == emyKingF : false);

    const auto kingBishopAdjacent =
      (abs(kingR - bishopR) == 1) or (abs(kingF - bishopF) == 1);

    BucketProbe::emit({
      {"defKingOnEdge",     int((king & EdgeSquares) != 0)},
      {"distBtwKings2",      distBtwKings == 2},
      {"kingBishopAdjacent", int(kingBishopAdjacent)},
      {"defenderToMove",    sideAdvantage},
      // {"bishopColorMatch",  int(bishopColorMatch)},
      {"kingsAlign", int(kingsAlign)}
      // {"defKingMove",       int(defKingHasMove)},
      // {"rookAttacksBishop", int(rookAttacksBishop)},
      // {"kingBishopDist",    chebyshevDistance(kingSq, bishopSq)},
      // {"rookAttacksKing",   int(rookAttacksKing)}
    });
  }

  return true;
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
  }

  return false;
}
