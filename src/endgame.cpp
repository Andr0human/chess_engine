
#include "endgame.h"
#include "attacks.h"

using plt::passedPawnMasks;


// TODO: KRRK, KRNK

static int
chebyshevDistance(Square s1, Square s2)
{
  int rank1 = s1 >> 3, file1 = s1 & 7;
  int rank2 = s2 >> 3, file2 = s2 & 7;
  return std::max(abs(rank1 - rank2), abs(file1 - file2));
}

Bitboard
getRank1(uint8_t color)
{ return color == WHITE ? Rank1 : Rank8; }

Bitboard
getRank2(uint8_t color)
{ return color == WHITE ? Rank2 : Rank7; }

Bitboard
getRank8(uint8_t color)
{ return color == WHITE ? Rank8 : Rank1; }

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

  const Bitboard    pawn = pos.getPiece(side, PAWN);
  const Bitboard  myKing = pos.getPiece(side, KING);
  const Bitboard emyKing = pos.getPiece(emySide, KING);

  const int side2move = pos.color;
  const int incFactor = 2 * side - 1;

  const Square pawnSq    = squareNo(pawn   );
  const Square myKingSq  = squareNo(myKing );
  const Square emyKingSq = squareNo(emyKing);

  const int pawnR   = pawnSq   >> 3;
  const int myKingR = myKingSq >> 3;

  if ((side2move == emySide) and
      (pawn & rank2to6[side]) and
      (side == WHITE ? myKingR <= pawnR - 1 : myKingR >= pawnR + 1) and
      (passedPawnMasks[side][pawnSq] & emyKing)
  ) return true;

  // if ((side2move == emySide) and
  //     (pawn & rank4to7[side]) and
  //     (passedPawnMasks[side][pawnSq] & emyKing) and
  //    !((passedPawnMasks[WHITE][pawnSq - 8] | passedPawnMasks[BLACK][pawnSq]) & myKing)
  // ) return true;

  if ((side2move == side) and
      (pawn & rank3to5[side]) and
      (side == WHITE ? myKingR <= pawnR - 1 : myKingR >= pawnR + 1) and
      (passedPawnMasks[side][pawnSq] & emyKing)
  ) return true;

  if ((pawnSq + 8 * incFactor == emyKingSq) and
      (pawnSq - 8 * incFactor ==  myKingSq) and
      ((pos.color == emySide) or (!(emyKing & Rank18) and (pos.color == side)))
    ) return true;

  if ((pawnSq + 8 * incFactor == emyKingSq) and
      (pawn & (AllSquares ^ FileAH)) and
      ((pawnSq - 7 * incFactor == myKingSq) or (pawnSq - 9 * incFactor == myKingSq)) and
      ((pos.color == side) or (!(emyKing & Rank18) and (pos.color == emySide)))
    ) return true;

  if ((pawn & (AllSquares ^ FileAH)) and
      ((pawnSq + 1 == myKingSq) or (pawnSq - 1 == myKingSq)) and
      ((pawnSq + 16 * incFactor == emyKingSq) and
      ((pos.color == emySide)))
    ) return true;

  if ((pawnSq +  8 * incFactor ==  myKingSq) and
      (pawnSq + 24 * incFactor == emyKingSq) and
      ((pos.color == side) and !(emyKing & Rank18))
    ) return true;

  // Defender-king blockade draw: the defending king sits exactly two squares
  // directly in front of the pawn (same file) and it is the attacker's move.
  //   - attacker king one rank behind the pawn (d3/e3/f3 for a pawn on e4):
  //     always a draw (defender holds the opposition / stalemate resource).
  //   - attacker king beside the pawn (d4/f4): a draw unless the defender is
  //     forced onto a non-rook-file promotion square, which is the only winning
  //     break; on the a/h file that square is still the rook-pawn corner draw.
  // Complements the blocks above which cover this geometry with the defender to move.
  if ((side2move == side) and (emyKingSq == pawnSq + 16 * incFactor))
  {
    const int pawnF    = pawnSq   & 7;
    const int myKingF  = myKingSq & 7;
    const int fileDist = abs(myKingF - pawnF);

    const bool behindKing = (myKingR == pawnR - incFactor) and (fileDist <= 1);
    const bool besideKing = (myKingR == pawnR)             and (fileDist == 1);

    if (behindKing)
      return true;

    if (besideKing and (!(emyKing & Rank18) or (emyKing & FileAH)))
      return true;
  }

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
  Bitboard bishopCapMask = attackSquares<BISHOP>(bishopSq, myKing);

  if (pos.count<WHITE, ALL>() == 1)
  {
    // Pawn and bishop are of different side
    const Bitboard   pawn = pos.getPiece(emySide, PAWN);
    const Square   pawnSq = squareNo(pawn);

    Bitboard extEmyKingCapMask = 0;

    {
      const Bitboard emyKingLegalMask = emyKingCapMask & ~(occupied | bishopCapMask | kingCapMask);
      if ((emyKingLegalMask == 0) and
          (side2move == side) and
          (attackSquares<BISHOP>(emyKingSq, occupied) & bishopCapMask)
      ) return false;

      if ((side2move == side) and
          (emyKing & FileAH) and
          (emyKing & getRank1(emySide)) and
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

    if ((bishop | myKing) & pawnMask)
      return true;

    if ((bishopCapMask & pawnMask & ~emyKingCapMask) and
        ((side2move == side) or ((side2move == emySide) and (bishop & ~extEmyKingCapMask)))
    ) return true;

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

      if (intersection & ~getRank8(emySide)) {
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

  // Not a theoretical draw if, pawn is protected by bishop or king
  // or is closer to promo square than enemy king
  // or the pawn can reach a square above protected by its king
  if ((pawn & kingCapMask) or
      (pawn & bishopCapMask) or
      ((side == WHITE ? pawnR > emyKingR : pawnR < emyKingR) and !(plt::pawnMasks[side][pawnSq] & bishop)) or
      (!(plt::pawnMasks[side][pawnSq] & occupied) and (attackSquares<KING>(pawnSq + 8 * (2 * side - 1), 0) & myKing))
  ) return false;

  if ((pawn & FileAH) and
      (emyKing & pawnMask) and
      (side == WHITE ? emyKingR > kingR + 1 : emyKingR < kingR - 1) and
      (((cornerMask & WhiteSquares) and !(bishop & WhiteSquares))
    or ((cornerMask & BlackSquares) and !(bishop & BlackSquares)))
  ) return true;

  // If pawn is attacked by enemy king and our king cannot support the pawn
  if ((pawn & emyKingCapMask) and !(kingCapMask & attackSquares<KING>(pawnSq, 0) & ~emyKingCapMask)) {
    // If bishop is just above it
    if ((plt::pawnMasks[side][pawnSq] & bishop))
      return true;
    
    // If bishop cannot support it
    if (!(pawn & getRank2(side)) and
       (!(bishopCapMask & plt::pawnMasks[side][pawnSq]) and !(bishopCapMask & attackSquares<BISHOP>(pawnSq, occupied)))
    ) return true;
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

  if (!(emyKing & (Rank18 | FileAH)) and
      !(attackSquares<ROOK>(squareNo(pos.getPiece(side, ROOK)), 0) & bishop) and
      (chebyshevDistance(kingSq, bishopSq) > 3) and
      (abs(emyKingSqR - bishopSqR) > 1 and abs(emyKingSqF - bishopSqF) > 1)
  ) return true;

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

    if (isEndgame<Endgames::KBBK>(pos))
      return Endgame<Endgames::KBBK>(pos);

    if (isEndgame<Endgames::KBNK>(pos))
      return Endgame<Endgames::KBNK>(pos);

    if (isEndgame<Endgames::KRBK>(pos))
      return Endgame<Endgames::KRBK>(pos);
  }

  return false;
}

