
#include "endgame_probe.h"
#include "endgame_utils.h"
#include "attacks.h"

using plt::passedPawnMasks;
using plt::ruleOfSquares;

// Table lookups -- see the metric note in lookup_table.h.
using plt::chebyshevDistance;
using plt::manhattanDistance;


// TODO: KRRK, KRNK


// --- Won-side scoring -------------------------------------------------------
//
// A recognizer that proves a win reports how good the win is, not merely that
// one exists. The number *replaces* the evaluation rather than adding to it, so
// the base is the material the win converts into, and every term around it
// exists only to order one won position against another.
//
// That ordering is the point. Without it every won KPK scores the same, search
// sees no reason to prefer promoting now over shuffling, and a proven win turns
// into a 50-move draw. The spread is deliberately small (~100cp against a
// four-figure base): it has to break ties between won positions without ever
// coming close to making one look drawn.
//
// A recipe is a base plus a list of terms, and both are template arguments. The
// recognizer that calls in was selected *by* material, so everything the
// material fixes -- what the win is worth, which terms even apply -- is already
// known at compile time and none of it should cost a runtime test. The base is
// what the winner ends up holding; the terms are the spread around it. A term
// left off the list is not merely skipped, it is never compiled.


// The square the pawn is walking to.
inline Square
promotionSquare(const Color side, const Square pawnSq)
{ return static_cast<Square>((side == WHITE ? 56 : 0) + (pawnSq & 7)); }

// Winner-relative to side-to-move relative, matching alphaBeta's negamax and
// evaluate()'s `* (2 * pos.color - 1)`. Applied once, by the fold below, so no
// recognizer and no term has to get the sign right itself.
inline Score
relativeToMover(const ChessBoard& pos, const Color side, const Score score)
{ return (side == pos.color) ? score : -score; }


// Unqualified here only -- it keeps a recipe readable at the call site, and the
// scoped names stay scoped everywhere else. DirectionalTerm itself is in types.h
// alongside Endgames.
using enum DirectionalTerm;

// One term, one question. Every term is winner-relative (bigger is better for
// `side`) and bounded well inside the material base -- keeping them on a common
// scale is what lets one beta-cut threshold mean the same thing in every
// signature. A term reads whatever it needs off `pos` itself rather than taking
// derived arguments, so recipes stay a flat list with no ordering rules.
//
// The primary is deleted: naming a term that has no body is a build error, not
// a silent zero.
template <DirectionalTerm t>
inline Score
termScore(const ChessBoard&, const Color) = delete;

// The pawn is on the queening square's file, so the distance left to travel is
// just ranks: 1 on the 7th, 6 on the 2nd.
template <>
inline Score
termScore<PAWN_MARCH>(const ChessBoard& pos, const Color side)
{
  const Square pawnSq = squareNo(pos.getPiece(side, PAWN));

  return 10 * (7 - chebyshevDistance(pawnSq, promotionSquare(side, pawnSq)));
}

// Reward the defender being far from the queening square, penalize the escort
// still being far from it. Weighted below PAWN_MARCH -- a rank of pawn advance
// outweighs two squares of king travel, the right priority once the promotion
// is known to be safe.
template <>
inline Score
termScore<KING_RACE>(const ChessBoard& pos, const Color side)
{
  const Square   pawnSq = squareNo(pos.getPiece(side, PAWN));
  const Square  queenSq = promotionSquare(side, pawnSq);
  const Square  myKingSq = squareNo(pos.getPiece( side, KING));
  const Square emyKingSq = squareNo(pos.getPiece(~side, KING));

  return 4 * chebyshevDistance(emyKingSq, queenSq)
       - 2 * chebyshevDistance( myKingSq, queenSq);
}

// The mating-net term: a forced mate is the winner's king closing on the
// loser's, so reward them being close. No signature composes this yet -- it is
// what the pawnless wins (KBBK, KBNK, KRBK) will want, over a base near
// VALUE_MATE rather than a piece sum, once any of them proves a win.
template <>
inline Score
termScore<KING_PROXIMITY>(const ChessBoard& pos, const Color side)
{
  const Square  myKingSq = squareNo(pos.getPiece( side, KING));
  const Square emyKingSq = squareNo(pos.getPiece(~side, KING));

  return 10 * (7 - chebyshevDistance(myKingSq, emyKingSq));
}


// Base a KPK proven win is scored from, before the recipe's terms adjust it.
// QueenValueMg says what the position is worth once the pawn queens; the terms
// then spread roughly +/-50 around it to order won positions against each other.
constexpr Score KPK_WIN_BASE = QueenValueMg;

// calcDirectionalScore<base, terms...>(pos, side) -- the one call a recognizer
// makes on a path where it has *proven* `side` wins. It names its recipe and
// nothing else: no metric is invoked by hand, no weight is repeated, and the
// sign is handled here. An empty term list folds to the bare base.
template <Score base, DirectionalTerm... terms>
inline Score
calcDirectionalScore(const ChessBoard& pos, const Color side)
{ return relativeToMover(pos, side, (base + ... + termScore<terms>(pos, side))); }

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
Endgame<Endgames::KNNK>(const ChessBoard& pos, Score&)
{
  // A knight apiece (KNKN): drawn, bar the cornered-king geometry.
  if (pos.count<WHITE, KNIGHT>() == 1)
  {
    return !(cornerBlockedByOwnPiece(pos.piece<WHITE, KING>(), pos.piece<WHITE, KNIGHT>())
          or cornerBlockedByOwnPiece(pos.piece<BLACK, KING>(), pos.piece<BLACK, KNIGHT>()));
  }

  // Two knights against a bare king. Mate cannot be forced, so the position is
  // drawn unless the defender already stands in a mating net. Every won
  // position in this signature's call set (308 of 5.7M) sits inside the same
  // four conditions: the knight side to move, the bare king on an edge, the
  // kings exactly two squares apart, and at most one flight square left -- a
  // king with two ways out walks free before any mate arrives. Outside that
  // the draw claim is safe; inside it, defer to eval.
  //
  // Cheapest test first: the escape count at the bottom costs four table
  // lookups, the three gates above it cost one each.
  const Color atkSide = pos.count<WHITE, KNIGHT>() ? WHITE : BLACK;

  if (pos.color != atkSide)
    return true;

  const Bitboard defKing = pos.getPiece(~atkSide, KING);
  const Bitboard atkKing = pos.getPiece( atkSide, KING);

  if (!(defKing & EdgeSquares))
    return true;

  const Square defKingSq = squareNo(defKing);
  const Square atkKingSq = squareNo(atkKing);

  if (chebyshevDistance(atkKingSq, defKingSq) != 2)
    return true;

  // Squares the bare king may not step onto. A knight's own square counts as
  // covered even when the king could legally take it: that only over-counts
  // the net, which errs towards *not* claiming the draw.
  const Bitboard knights = pos.getPiece(atkSide, KNIGHT);
  const Bitboard covered = plt::kingMasks[atkKingSq]
                         | plt::knightMasks[lsbIndex(knights)]
                         | plt::knightMasks[msbIndex(knights)]
                         | knights;

  return popCount(plt::kingMasks[defKingSq] & ~covered) > 1;
}

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
     (chebyshevDistance(emyKingSq, pawnSq) == 1) and
     (chebyshevDistance(myKingSq, pawnSq) > 1)
  ) return true;

  if (!sideAdvantage and
     (pawn & FileAH) and
     (myKing & (passedPawnMasks[side][pawnSq] & plt::lineMasks[pawnSq])) and
     (abs(kingFileDiff) == 2 or abs(kingFileDiff) == 3) and
     (side == WHITE
      ? emyKingR >= myKingR - 1 - myKingOnRank8
      : emyKingR <= myKingR + 1 + myKingOnRank8)
  ) return true;

  const int ruleOfSquareIndex = getRuleOfSquareIndex(pos, side, pawnSq);

  // Defender outside the rule of the square: the pawn walks in unescorted.
  if (emyKing & ~plt::ruleOfSquares[side][ruleOfSquareIndex]) {
    dirScore = calcDirectionalScore<KPK_WIN_BASE, PAWN_MARCH, KING_RACE>(pos, side);
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

  // Own king two ranks ahead on the pawn's file: the key squares are taken and
  // the defender can be outflanked whichever way it steps.
  //
  // Not for a rook pawn. There the king in front of its own pawn is the losing
  // -- rather, the *drawing* -- pattern: the defender only has to reach the
  // corner, and the file-fold means there is no side to be outflanked from. All
  // 112 drawn positions this test admits are a-pawns, so the gate is exact, and
  // it lets those fall through to the rook-pawn corner draw below, which is the
  // rule that judges them correctly.
  if (!(pawn & FileAH) and (myKingR == pawnR + 2 * incFactor) and (myKingF == pawnF))
  {
    dirScore = calcDirectionalScore<KPK_WIN_BASE, PAWN_MARCH, KING_RACE>(pos, side);
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
    const Square queenSq = promotionSquare(side, pawnSq);
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
    const Square promoSq = promotionSquare(side, pawnSq);

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
  // A bishop apiece (KBKB): drawn, bar the cornered-king geometry.
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
Endgame<Endgames::KBNK>(const ChessBoard& pos, Score&)
{
  // A minor apiece (KBKN): drawn, bar the cornered-king geometry.
  if (pos.count<WHITE, ALL>() == 1)
  {
    const Bitboard wMinor = pos.piece<WHITE, BISHOP>() | pos.piece<WHITE, KNIGHT>();
    const Bitboard bMinor = pos.piece<BLACK, BISHOP>() | pos.piece<BLACK, KNIGHT>();

    return !(cornerBlockedByOwnPiece(pos.piece<WHITE, KING>(), wMinor)
          or cornerBlockedByOwnPiece(pos.piece<BLACK, KING>(), bMinor));
  }

  // Bishop and knight against a bare king: won, unless the defender is to move
  // and can pick off a minor its own king already touches. Wholly gated out of
  // search today -- no capture is available in a gated call set -- but it fires
  // on 1.17M positions once the capture gate comes out.
  const Color side = pos.count<WHITE, ALL>() > 0 ? BLACK : WHITE;
  const bool defToMove = side == pos.color;

  const Bitboard king      = pos.getPiece( side, KING);
  const Bitboard emyKing   = pos.getPiece(~side, KING);
  const Square   kingSq    = squareNo(king);
  const Square   emyKingSq = squareNo(emyKing);

  const Bitboard bishopMask = attackSquares<BISHOP>(squareNo(pos.getPiece(~side, BISHOP)), emyKing);
  const Bitboard knightMask = attackSquares<KNIGHT>(squareNo(pos.getPiece(~side, KNIGHT)), emyKing);

  const auto hangingPiece =
    pos.getPiece(~side, ALL) & ~plt::kingMasks[emyKingSq] & ~bishopMask & ~knightMask;

  if (defToMove and (plt::kingMasks[kingSq] & hangingPiece))
    return true;

  return false;
}


template <>
bool
Endgame<Endgames::KRBK>(const ChessBoard& pos, Score&)
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

  // Every rule below is keyed on one of these five distances plus the two
  // structural coordinates (kingMob, badCornerD) defined further down.
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

  // Two different rook-to-bishop relations, and the rules below distinguish
  // them: rookHitsBishop is the empty-board ray ("the bishop stands on a line
  // the rook could work with"), rookOnBish the real attack through blockers.
  const Bitboard rookNow    = attackSquares<ROOK>(rookSq, occupied);
  const bool rookHitsBishop = attackSquares<ROOK>(rookSq, 0) & bishop;
  const bool rookOnBish     = rookNow & bishop;

  // ------------------------------------------------------------------
  // Decided regions: bail out rather than claim a draw.
  // ------------------------------------------------------------------

  // Pinned bishop the defence cannot save. The rook truly attacks the bishop
  // and, with the bishop lifted, the same ray reaches the defending king --
  // squares revealed by removing a piece lie strictly beyond it on the one ray
  // it stood on, so this is exactly "rook -> bishop -> king, nothing between".
  // A bishop pinned on a rank/file has no legal move whatsoever.
  //
  // With its king more than a move away from guarding it (> 2 => the king cannot
  // even reach an adjacent square in one), the frozen bishop drops and the rook
  // side is left with KRK -- decided. The side-to-move clause is the stalemate
  // escape: with the DEFENDER to move and the enemy king two squares off, the
  // defender can walk into the corner net and answer RxB with stalemate
  // (8/8/8/8/8/8/k3bR2/2K5 b: Ka1! and Rxe2 is stalemate). Data-mined against
  // the perfect KRKB oracle -- 29,716 decided positions, zero drawn.
  const Bitboard rookXray = attackSquares<ROOK>(rookSq, occupied ^ bishop);

  if (rookOnBish and ((rookXray & ~rookNow) & king) and
     (distBtwKingAndBish > 2) and
     (!defToMove or (distBtwKings > 2))
  ) return false;

  // Hanging bishop -- the KRKB analogue of the KPBK guard. Everything below
  // reasons about a bishop that stays on the board, so a bishop the rook side
  // can simply take is out of scope: defer to eval instead of claiming the draw.
  // Both qualifiers matter: only the rook side to move can execute the capture,
  // and a bishop its own king defends is not worth taking. Measured over the
  // full oracle: inert inside the call gate while clearing the whole
  // 102,228-position gate-off residue, and it costs no gate-off draw.
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

  // Bishop on its king's rank or file with the attacker to move: the alignment
  // the rook wins against. Rank and file are the same rule read along the shared
  // line, so they share one body -- perp is the coordinate ACROSS that line (the
  // file when king and bishop share a rank, the rank when they share a file).
  // The two cases are exclusive: matching both would put them on one square.
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

  // ------------------------------------------------------------------
  // Draw regions. Every rule below was data-mined against the perfect KRKB
  // oracle and is FALSE-DRAW-free over the full 11.3M-position sweep.
  // ------------------------------------------------------------------

  const int kingEdgeD   = std::min(std::min(kingR, 7 - kingR),
                                   std::min(kingF, 7 - kingF));
  const int bishopEdgeD = std::min(std::min(bishopR, 7 - bishopR),
                                   std::min(bishopF, 7 - bishopF));
  const bool offRim = (kingEdgeD >= 1) and (bishopEdgeD >= 1);

  // The classic KRKB defence rests on a colour match: a defending king standing
  // on its own bishop's colour can be shielded by it, so the rook never gets the
  // skewer that wins the piece.
  const bool kingOnBishColour =
    bool(king & WhiteSquares) == bool(bishop & WhiteSquares);

  // Philidor's colour rule as a coordinate: the corner that loses KRKB is the one
  // whose colour matches the BISHOP's, so measure the defending king against those
  // two squares. badCornerD == 7 is exactly "the king stands in the safe corner"
  // -- from a good corner both bad ones are a full board away, and no other square
  // reaches 7.
  const Bitboard badCorners =
    CornerSquares & ((bishop & WhiteSquares) ? WhiteSquares : BlackSquares);

  int badCornerD = 8;
  for (Bitboard c = badCorners; c != 0; c &= c - 1)
    badCornerD = std::min(badCornerD, chebyshevDistance(kingSq, squareNo(c & -c)));

  // Bishop with its own king: the rook cannot win a piece the king guards, so
  // what decides these is how close the ATTACKING king has got and how far the
  // kings stand apart. Ten mined boxes, ~1.0M draws.
  if (distBtwKingAndBish <= 2)
  {
    const bool bishopBeside = distBtwKingAndBish == 1;

    // Kings far apart -- the rook can never break in before the defence re-forms.
    if (distBtwKings > 4)
      return true;

    // Neither piece on the rim and the attacking king not on top of them: the
    // textbook held draw, and the single largest pure box the cube contains.
    if (offRim and (atkKingBishD >= 3))
      return true;

    if (offRim and bishopBeside and (atkKingBishD == 2))
      return true;

    // Attacking king still a long way off. Sole exception from range 5 out:
    // attacker to move with the rook already on the bishop's line and the
    // defending king pressed to the edge -- the trap-against-the-edge setup,
    // and it wins (8 positions).
    if ((atkKingBishD >= 5) and
       !(!defToMove and rookHitsBishop and (kingEdgeD == 0)))
      return true;

    // One square nearer still holds on the defender's move with the rook off the
    // bishop's line, or -- with the bishop two off -- when the king sits on the
    // wrong colour for the attacker's net.
    if ((atkKingBishD == 4) and
       ((defToMove and !rookHitsBishop) or (!bishopBeside and !kingOnBishColour)))
      return true;

    // Bishop guarded by its own king, away from the bad corners, attacking king
    // at middle range: it cannot both approach and keep the rook useful.
    if (bishopBeside and (badCornerD >= 2) and
       ((atkKingBishD == 4) or (atkKingBishD == 5) or
        (defToMove and (atkKingBishD == 3))))
      return true;

    // Kings exactly four apart with the bishop two off -- off the rim, or with
    // the colour mismatch working for the defence.
    if (!bishopBeside and (distBtwKings == 4) and
       ((kingEdgeD >= 1) or (defToMove and !kingOnBishColour)))
      return true;

    // Bishop right beside its king, off the rim, rook not yet on its line. The
    // king-separation bound is asymmetric: with the attacker to move it wins a
    // tempo at exactly 3, so it needs 4.
    if (bishopBeside and (kingEdgeD >= 1) and !rookHitsBishop and
       (distBtwKings > 2 + !defToMove))
      return true;

    if (bishopBeside and defToMove and (distBtwKings == 4) and (kingRookD > 1))
      return true;
  }

  // A bishop three squares off is still close enough to shield -- provided the
  // colour matches, or (defender to move, kings far apart) the rook is out of
  // reach. At distKingBish 3 with the attacking king 5-7 away the wrong-colour
  // half carries 6,844 decided positions while the right-colour half carries none.
  if (distBtwKingAndBish == 3)
  {
    if (kingOnBishColour and !rookHitsBishop and (atkKingBishD > 3))
      return true;

    if (defToMove and (distBtwKings >= 5) and
       (!kingOnBishColour or (kingRookD > 3)))
      return true;
  }

  // King off the rim, bishop loose but two clear of it in BOTH coordinates, and
  // the attacking king far from the bishop.
  if ((kingEdgeD >= 1) and !rookHitsBishop and (atkKingBishD > 3) and
     (abs(kingR - bishopR) > 1) and (abs(kingF - bishopF) > 1))
    return true;

  // The loose-bishop region indexed by the defending king's distance from the
  // edge: well off the rim the mating net has nothing to press the king against.
  // Ranges 4-5 are the exception and not noise -- close enough to help the rook
  // trap the bishop, far enough that the defence cannot chase the king off.
  if (defToMove and !rookOnBish and (kingEdgeD >= 2) and
     ((atkKingBishD == 2) or (atkKingBishD == 3) or (atkKingBishD >= 6)))
    return true;

  // King at the rook's throat with the rook's own king out of reach: the rook
  // must keep stepping away, so the attack never gets organised. Asymmetric in
  // both coordinates -- a rook the king actually touches holds from 3 apart with
  // the defender to move but needs 5 with the attacker to move (a free tempo to
  // untangle), and at distance 2 the king is merely harassing. ~845k draws.
  if (kingRookD == 1)
  {
    if (defToMove ? ((distBtwKings > 2) or (atkKingRookD > 1))
                  : ((distBtwKings > 4) or (atkKingRookD > 3)))
      return true;
  }
  else if (defToMove and (kingRookD == 2) and
          ((distBtwKings > 4) or (!rookHitsBishop and (atkKingRookD > 2))))
    return true;

  // How many squares the defending king actually has. This is the coordinate the
  // distance features could not express: the rook wins KRKB by confining the king
  // and then squeezing, so a king whose field is still intact is a king the attack
  // has not begun to work on. Squares are counted the way the king may really use
  // them -- the rook's ray is taken THROUGH the king (a king does not shield
  // itself), its own bishop is not a destination, and the rook is only edible when
  // its own king does not defend it.
  const Bitboard rookThruKing = attackSquares<ROOK>(rookSq, occupied ^ king);
  Bitboard kingEsc = kingMask & ~emyKingMask & ~bishop & ~rookThruKing;
  if (emyKingMask & rook) kingEsc &= ~rook;

  const int kingMob = popCount(kingEsc);

  // ~850k draws, every one of them with the defender to move. The switch is not
  // cosmetic: kingMob behaves as an exact LABEL rather than a threshold, because
  // the intermediate values are the fields a rook has already started to cut
  // down. 5 is an untouched rim king and 8 an untouched king off the rim, both
  // clean -- while 4 and 6 carry thousands of decided positions.
  if (defToMove)
  {
    switch (kingMob)
    {
      // Boxed in, but still off the bad corners with the attacking king not yet
      // arrived.
      case 0: case 1: case 2:
        if ((distBtwKings >= 3) and (badCornerD >= 2) and (badCornerD <= 6))
          return true;
        if ((kingMob == 1) and (distBtwKings >= 5))
          return true;
        break;

      // Three squares: either tucked into the SAFE corner (badCornerD == 7, the
      // textbook draw) or sitting in the bad one with the enemy king still too
      // far to build the net.
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

      // A rim king with its whole field still free.
      case 5:
        if ((distBtwKings >= 5) or
           ((distBtwKings == 4) and (badCornerD != 4)))
          return true;
        if (!rookOnBish and (atkKingBishD >= 4))
          return true;
        break;

      // The awkward value -- a king one step off the rim with two squares gone.
      case 6:
        if (!rookOnBish and ((atkKingBishD >= 5) or (atkKingBishD == 2)))
          return true;
        break;

      // Off the rim with essentially nothing taken from it.
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

    const Square promoSq = promotionSquare(side, pawnSq);

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

// Which recognizers' directionalScore is trustworthy enough to fail high on.
// A signature qualifies only if a nonzero score means the win is *proven* by the
// recognizer itself. Where the score is merely a heuristic lean, leave the
// signature out: it can still raise alpha, but cutting on it would let search
// return a bound it cannot back up. Default is false, so a new recognizer opts
// in rather than out.
//
// "Proven" is a measurement, not the author's reading of the branch. `egvalidate`
// checks draw verdicts only, so a `dirScore = ...; return false` is a win claim
// nothing validates -- a false one lands in the missed-draw bucket, which is
// allowed to be enormous. To audit one branch, flip its `return false` to
// `return true` and re-run `egvalidate pieces <sig> oracle`: FALSE-DRAW is then
// exactly the positions it really wins, and any rise in agree-draw over the same
// build un-flipped is exactly the positions it gets wrong. Run it both gated and
// `nocapgate`, and re-run it after touching any earlier `return true`, which
// changes what reaches the branch at all.
//
// KPK is in because both its scoring paths measure clean, 0 false claims in both
// call sets: the enemy king outside the rule of the square (85,888 proven wins),
// and the key-square opposition (801). Neither was clean when it was first
// whitelisted -- the key-square test claimed 70 drawn rook-pawn positions until
// it was gated on `!(pawn & FileAH)`, and the rule-of-square test claimed 599
// ungated until the hanging-pawn draw at the top of Endgame<KPK> caught them.
template <Endgames e>
constexpr bool
cutsBeta()
{
  for (const auto eg : { Endgames::KPK })
    if (eg == e) return true;

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

  egp.signatureHit   = true;
  egp.scoreCutsBeta  = cutsBeta<e>();
  egp.positionHit    = Endgame<e>(pos, egp.directionalScore);
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
