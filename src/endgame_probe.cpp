
#include "endgame_probe.h"
#include "endgame_utils.h"
#include "attacks.h"

using plt::passedPawnMasks;
using plt::ruleOfSquares;

// Table lookups -- see the metric note in lookup_table.h.
using plt::chebyshevDistance;
using plt::manhattanDistance;


// TODO: KRRK, KRNK

// Endgame<E>(pos, dirScore) -- the per-signature verdict. The return value is the
// probe's positionHit: true == this exact position is a known theoretical draw.
// dirScore is the won-side score, written only when the recognizer proves the
// position is NOT a draw; recognizers that do not yet compute one leave it alone
// (the probe zero-initializes it) and take an unnamed parameter to say so.
template <Endgames e>
inline bool
Endgame(const ChessBoard& pos, Score& dirScore) = delete;

template <>
inline bool
Endgame<Endgames::KBK>(const ChessBoard&, Score&)
{ return true; }

template <>
inline bool
Endgame<Endgames::KNK>(const ChessBoard&, Score&)
{ return true; }

template <>
inline bool
Endgame<Endgames::KNNK>(const ChessBoard&, Score&)
{ return true; }

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

template <>
inline bool
Endgame<Endgames::KPBK>(const ChessBoard& pos, Score&)
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

    // Hanging bishop. The rest of this block reasons about a bishop that is still
    // on the board, so a bishop the pawn side can simply take is out of scope --
    // defer to eval rather than claim the draw.
    //
    // Both qualifiers are load-bearing. Only the pawn side to move can execute
    // the capture, and a bishop its own king defends cannot be taken by the bare
    // king at all; without either test the guard fires on positions where nothing
    // is actually hanging and costs 196,638 correctly-recognized draws inside the
    // call gate. With them it is exactly inert there (the gate already drops every
    // position it fires on) and still clears the whole 208,415 gate-off residue.
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
Endgame<Endgames::KPQK>(const ChessBoard& pos, Score&)
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

    // Hanging queen -- the KPQK member of the guard family. Everything below
    // reasons about a queen that stays on the board, so defer to eval when the
    // defending king or pawn can just take it.
    //
    // Unlike the KPBK/KRKB/KPKN/KPRK guards this one carries neither qualifier:
    // no side-to-move test and no defended-piece test. Measured, it does not
    // need them -- adding both leaves the gated tallies bit-identical
    // (242,454 / 136,604 / FALSE-DRAW 0) and recovers all of 32 draws gate-off,
    // so the extra tests would be noise. The reason the qualifiers carry no
    // weight here is that the queen side is a bare king: a queen its own king
    // defends is still a queen for nothing, and the defender's king and pawn
    // between them cover so little of the board that the unqualified form
    // barely over-fires.
    if (queen & (plt::kingMasks[kingSq] | plt::pawnCaptureMasks[side][pawnSq]))
      return false;

    // Rook-/bishop-pawn fortress. With the pawn on the 7th and its own king
    // holding the promotion square, the queen alone cannot make progress: an
    // a/h or c/f pawn hands the defender the stalemate resource that a b/g or
    // centre pawn lacks, so the win needs the attacking king -- and it is still
    // six ranks away. The defender-to-move bishop-pawn case tolerates one more
    // tempo of approach. The king may not sit *on* the promotion square of a
    // rook pawn while the attacker is to move: there it blocks its own pawn
    // with no flight square, and the queen mates instead of stalemating.
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

      // The four filters below reject a position because the queen bears on the
      // defending king along a file/diagonal/rank -- the skewer that wins the
      // new queen after the pawn promotes. That test is purely geometric, so it
      // also rejects positions where the skewer cannot be converted: with the
      // defending king already on the promotion square's doorstep and the
      // attacking king still out of range, there is no follow-up and the
      // ending is drawn regardless of the alignment.
      const bool queenLineDraw =
           (dkPromoDist == 1 and distanceBtwKings > daPromoDist)
        or (dkPromoDist == 1 and daPromoDist >= 4 and distanceBtwKings >= 4)
        or (dkPromoDist == 2 and daPromoDist >= 5 and distanceBtwKings >= 6);

      // A skewer along a file or rank is far weaker than one along a diagonal:
      // the promoted queen and the king sit on the same colour complex there,
      // so the diagonal pin has no parry while the orthogonal one is met by
      // interposing. With the king a knight's-move from the promotion square
      // and the attacking king still four away, the orthogonal alignment is
      // therefore not enough to win.
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

      // King and promo square share the a1-h8 diagonal (file - rank constant).
      // A bare `% 9` index test wrongly fires on file-wrapped differences that
      // happen to be multiples of 9, which breaks colour symmetry (the index
      // delta is not preserved under a vertical mirror).
      if ((myKingF - myKingR) == (pawnF - (promoSq >> 3)))
      {
        Bitboard mask = side == WHITE ? plt::downLeftMasks[kingSq] : plt::upRightMasks[kingSq];
        mask &= ~kingMask;
        if (queenMask & mask)
          return queenLineDraw;
      }

      // Same for the a8-h1 anti-diagonal (file + rank constant); `% 7` had the
      // identical file-wrap false-positive bug.
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

    // Condition-1 pre-check needed
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

    // Pawn on the 7th, its own king beside the promotion square, the queen not
    // yet bearing on that king, and the attacking king far away: the defence
    // simply shuffles between the pawn and the promotion square, and the lone
    // queen has no way to gain a tempo before its king arrives. The distance is
    // manhattan, not chebyshev: what makes the position holdable is the total
    // walk the attacking king still owes, so a diagonal approach must count as
    // nearer than a straight one of the same chebyshev length.
    //
    // Oracle-mined with bucket-probing over the residual: ten PURE-DRAW buckets
    // (4,655 draws, zero decided) forming one contiguous band in mdKK >= 5.
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
Endgame<Endgames::KBBK>(const ChessBoard& pos, Score&)
{
  if (pos.count<WHITE, BISHOP>() == 1)
    return true;

  const Bitboard bishops = pos.piece<WHITE, BISHOP>() | pos.piece<BLACK, BISHOP>();
  return !((bishops & WhiteSquares) and (bishops & BlackSquares));
}

template <>
inline bool
Endgame<Endgames::KBNK>(const ChessBoard& pos, Score&)
{ return pos.count<WHITE, ALL>() == 1; }


template <>
bool
Endgame<Endgames::KRBK>(const ChessBoard& pos, Score&)
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

  const int distBtwKings       = chebyshevDistance(kingSq, emyKingSq);
  const int distBtwKingAndBish = chebyshevDistance(kingSq, bishopSq );

  const Bitboard bishopMask  = attackSquares<BISHOP>(bishopSq , king);
  const Bitboard rookMask    = attackSquares< ROOK >(rookSq   , emyKing);
  const Bitboard kingMask    = attackSquares< KING >(kingSq   , 0);
  const Bitboard emyKingMask = attackSquares< KING >(emyKingSq, 0);

  const Bitboard rookDiag = attackSquares<BISHOP>(rookSq, 0);
  const bool rookHitsBishop = attackSquares<ROOK>(rookSq, 0) & bishop;

  // Hanging bishop -- the KRKB analogue of the KPBK guard. Everything below
  // reasons about a bishop that stays on the board, so a bishop the rook side
  // can simply take is out of scope: defer to eval instead of claiming the draw.
  //
  // Both qualifiers matter. Only the rook side to move can execute the capture,
  // and a bishop its own king defends is not worth taking -- the recapture
  // leaves K vs K. Measured over the full oracle: exactly inert inside the call
  // gate (tallies identical to no guard) while clearing the whole
  // 102,228-position gate-off residue, and it costs no gate-off draw either --
  // every claim it removes was a false one.
  if (!sideAdvantage and
     (rookMask & bishop) and
    !(kingMask & bishop)
  ) return false;

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

  if (!(king & EdgeSquares) and
      !rookHitsBishop and
      (chebyshevDistance(emyKingSq, bishopSq) > 3) and
      (abs(kingR - bishopR) > 1 and abs(kingF - bishopF) > 1)
  ) return true;

  // Center fortress (data-mined against the perfect KRKB oracle, FALSE-DRAW-free
  // over the full sweep): the defending king is off the edge with its bishop
  // guarded by that king (so the rook cannot win it) -> a held draw. The
  // king-separation bound is asymmetric: with the defender to move the kings need
  // only be >= 3 apart, but with the *attacker* to move it can win a tempo at
  // exactly 3, so it needs >= 4 (every dist-3 attacker-to-move case is a win).
  if (!(king & EdgeSquares) and
      (distBtwKingAndBish == 1) and
      (distBtwKings > 2 + !sideAdvantage) and
      !rookHitsBishop
  ) return true;

  // Kings far apart with the bishop hugging its own king: the rook can never
  // break in before the defence re-forms. Data-mined FALSE-DRAW-free over the
  // full KRKB oracle; independent of edge/side-to-move (both were irrelevant in
  // the residual PURE-DRAW buckets). Generalises the fortress rule above.
  if ((distBtwKingAndBish < 3) and (distBtwKings > 4))
    return true;

  // Off-edge, king two squares from its bishop, kings exactly four apart: a held
  // draw regardless of side to move. Data-mined against the perfect KRKB oracle
  // as two twin PURE-DRAW buckets (defender- and attacker-to-move, ~206k draws
  // combined, zero decided) that the far-apart rule above misses at distKings==4.
  if (!(king & EdgeSquares) and
      (distBtwKingAndBish == 2) and
      (distBtwKings == 4))
    return true;

  return false;
}

template <>
inline bool
Endgame<Endgames::KPNK>(const ChessBoard& pos, Score&)
{
  // Look from the KNIGHT side -- the defender fighting to hold the draw against
  // the passed pawn. Like KPQK this covers both material configs: the
  // opposite-side KN-vs-KP case (each side one non-king man) carries the draw
  // logic; same-side KPN-vs-K falls through to the terminal return false (trivial
  // win -- a safe missed-draw gap for now).
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
    // Pawn advancement counted from the ATTACKER's back rank: 2..7, higher = closer to promotion.
    const int  pawnRel = (emySide == WHITE) ? pawnR + 1 : 8 - pawnR;

    Bitboard kingCapMask    = attackSquares<KING>(myKingSq , 0);
    Bitboard emyKingCapMask = attackSquares<KING>(emyKingSq, 0);

    // Hanging knight -- the KPKN analogue of the KPBK guard, same two qualifiers:
    // only the pawn side to move can execute the capture, and a knight its own
    // king defends cannot be taken by the bare king at all.
    //
    // Unlike the KPBK and KRKB versions this one is NOT free gate-off. Bucketed
    // against the oracle by (claiming rule, hanging), every non-hanging bucket is
    // pure draw and the hanging ones hold all 20,533 gate-off FALSE-DRAWs -- but
    // also 92,786 genuine draws, because rules 1/2/7/8 below are king-based and
    // mostly survive losing the knight (they are 73-97% draw when it hangs; only
    // rule 5, the knight-forks-the-promotion-square rule, is 94% decided and
    // genuinely needs the knight). Splitting them apart needs a "drawn as bare
    // KPK" test, which is a coverage project, not a correctness one.
    //
    // Shipped as-is because it is exactly inert inside the call gate (gated
    // tallies bit-identical to no guard) and takes the gate-off FALSE-DRAW count
    // to 0, trading a bug for a safe missed-draw gap.
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

    // --- Draw rules first: claim the known draws before any carve-out filter, so
    // a filter can never steal a genuine draw from a rule below it. The
    // hanging-knight guard above is the one deliberate exception -- see its note
    // for what it costs the rules below. ---

    // Defender king blockades the promotion square (any pawn rank): the pawn can
    // never queen, so a held draw. Oracle-mined FALSE-DRAW-free.
    if (defKingPromoDist == 0)
      return true;

    // Unadvanced pawn with the defence in range: the pawn is still on the
    // attacker's 2nd/3rd rank, the defender king is near the promotion square,
    // and the king or knight is close enough to the pawn to blockade or win it.
    // Oracle-mined FALSE-DRAW-free over the full KPKN sweep.
    if (pawnRel <= 3 and defKingPromoDist <= 3 and
        std::min(defKingPawnDist, knightPawnDist) <= 3)
      return true;

    // Defender to move, unadvanced non-rook pawn, and the knight has a safe
    // (non-losing) move: the defender always has a holding move, so it holds the
    // draw. Rook pawns are excluded -- there the knight can be trapped in the
    // corner (the sibling bucket carries decided positions). Oracle-mined
    // FALSE-DRAW-free.
    if (pawnRel <= 3 and defToMove and legalKnightSquares and not pawnOnFileAH)
      return true;

    // Same holding pattern one rank further advanced (pawnRel 4), made safe by the
    // rule-of-the-square: the defender king is inside the pawn's promotion square,
    // so it catches the pawn. Without the king-in-square gate rank 4 leaks decided
    // positions; with it the bucket is pure. Oracle-mined FALSE-DRAW-free.
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

    // Everything else falls through: the pawn is advanced and the defence is not
    // in a recognized holding pattern -- treat as decided and defer to search.
    return false;
  }

  return false;
}

template <>
inline bool
Endgame<Endgames::KPRK>(const ChessBoard& pos, Score&)
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


// Signature test + verdict in one step: if pos has material signature E, mark the
// probe as a signature hit and run the recognizer. Returns whether E claimed the
// position's signature, so the dispatch below can stop at the first match.
template <Endgames e>
inline bool
tryEndgame(const ChessBoard& pos, EndgameProbe& egp)
{
  if (!isEndgame<e>(pos))
    return false;

  egp.signatureHit = true;
  egp.positionHit  = Endgame<e>(pos, egp.directionalScore);
  return true;
}


EndgameProbe
probeEndgame(const ChessBoard& pos)
{
  EndgameProbe egp{};

  const int pieceCount = pos.count<ALL>();

  if (pieceCount > 2)
    return egp;

  // Bare kings -- no signature table entry needed, the draw is unconditional.
  if (pieceCount == 0)
  {
    egp.signatureHit = true;
    egp.positionHit  = true;
    return egp;
  }

  if (pieceCount == 1)
  {
    if (tryEndgame<Endgames::KPK>(pos, egp)) return egp;
    if (tryEndgame<Endgames::KBK>(pos, egp)) return egp;
    if (tryEndgame<Endgames::KNK>(pos, egp)) return egp;

    return egp;
  }

  if (tryEndgame<Endgames::KNNK>(pos, egp)) return egp;
  if (tryEndgame<Endgames::KPBK>(pos, egp)) return egp;
  if (tryEndgame<Endgames::KPQK>(pos, egp)) return egp;
  if (tryEndgame<Endgames::KBBK>(pos, egp)) return egp;
  if (tryEndgame<Endgames::KBNK>(pos, egp)) return egp;
  if (tryEndgame<Endgames::KRBK>(pos, egp)) return egp;
  if (tryEndgame<Endgames::KPNK>(pos, egp)) return egp;
  if (tryEndgame<Endgames::KPRK>(pos, egp)) return egp;

  return egp;
}


bool
isTheoreticalDraw(const ChessBoard& pos)
{ return probeEndgame(pos).positionHit; }


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
