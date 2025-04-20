
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

  if ((side2move == emySide) and
      (pawn & rank4to7[side]) and
      (passedPawnMasks[side][pawnSq] & emyKing) and
     !((passedPawnMasks[WHITE][pawnSq - 8] | passedPawnMasks[BLACK][pawnSq]) & myKing)
  ) return true;

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
  const Bitboard bishop   = pos.getPiece(side, BISHOP);
  const Square   bishopSq = squareNo(bishop);

  if (pos.count<WHITE, ALL>() == 1)
  {
    // Pawn and bishop are of different side
    const Bitboard pawn = pos.getPiece(emySide, PAWN);
    const Square pawnSq = squareNo(pawn);

    Bitboard pawnMask = passedPawnMasks[emySide][pawnSq] & plt::lineMasks[pawnSq];

    if ((bishop | pos.getPiece(side, KING)) & pawnMask)
      return true;

    if (attackSquares<BISHOP>(bishopSq, 0) & pawnMask)
      return true;

    if (side2move == emySide)
      pawnMask ^= pawnMask & plt::pawnMasks[emySide][pawnSq];

    while (pawnMask)
    {
      const Square sq = nextSquare(pawnMask);

      const Bitboard bishopSquares = attackSquares<BISHOP>(bishopSq, occupied) & attackSquares<BISHOP>(sq, occupied);
      const Square   emyKingSq = squareNo(pos.getPiece(emySide, KING));

      if (bishopSquares & ~plt::kingMasks[emyKingSq])
        return true;
    }

    return false;
  }

  const Bitboard pawn = pos.getPiece(side, PAWN);
  const Square pawnSq = squareNo(pawn);

  Bitboard pawnMask = passedPawnMasks[side][pawnSq] & plt::lineMasks[pawnSq];
  Bitboard cornerMask = pawnMask & Rank18;

  if (((cornerMask & WhiteSquares) and (bishop & WhiteSquares))
   or ((cornerMask & BlackSquares) and (bishop & BlackSquares))) return false;

  if ((pawn & FileAH) and (pos.getPiece(emySide, KING) & pawnMask))
    return true;

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

  if ((attackSquares<ROOK>(emyKingSq, 0) & bishop) == 0)
  {
    if (!(emyKing & (Rank18 | FileA | FileH)))
      return true;

    if (chebyshevDistance(kingSq, emyKingSq) >= 4)
      return true;
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

    if (isEndgame<Endgames::KBBK>(pos))
      return Endgame<Endgames::KBBK>(pos);

    if (isEndgame<Endgames::KBNK>(pos))
      return Endgame<Endgames::KBNK>(pos);

    if (isEndgame<Endgames::KRBK>(pos))
      return Endgame<Endgames::KRBK>(pos);
  }

  return false;
}

