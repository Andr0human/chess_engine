
#include "endgame.h"
#include "attacks.h"

using plt::PassedPawnMasks;


// TODO: KRRK, KRNK

static int
ChebyshevDistance(Square s1, Square s2)
{
  int rank1 = s1 >> 3, file1 = s1 & 7;
  int rank2 = s2 >> 3, file2 = s2 & 7;
  return std::max(abs(rank1 - rank2), abs(file1 - file2));
}


constexpr Bitboard Rank2to6[COLOR_NB] = {
  AllSquares ^ (Rank18 | Rank2),
  AllSquares ^ (Rank18 | Rank7)
};

constexpr Bitboard Rank3to5[COLOR_NB] = {
  Rank4 | Rank5 | Rank6,
  Rank3 | Rank4 | Rank5
};

constexpr Bitboard Rank4to7[COLOR_NB] = {
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

  const Bitboard    pawn = pos.get_piece(side, PAWN);
  const Bitboard  myKing = pos.get_piece(side, KING);
  const Bitboard emyKing = pos.get_piece(emySide, KING);

  const int side2move = pos.color;
  const int incFactor = 2 * side - 1;

  const Square pawnSq    = SquareNo(pawn   );
  const Square myKingSq  = SquareNo(myKing );
  const Square emyKingSq = SquareNo(emyKing);

  const int pawnR   = pawnSq   >> 3;
  const int myKingR = myKingSq >> 3;

  if ((side2move == emySide) and
      (pawn & Rank2to6[side]) and
      (side == WHITE ? myKingR <= pawnR - 1 : myKingR >= pawnR + 1) and
      (PassedPawnMasks[side][pawnSq] & emyKing)
  ) return true;

  if ((side2move == emySide) and
      (pawn & Rank4to7[side]) and
      (PassedPawnMasks[side][pawnSq] & emyKing) and
     !((PassedPawnMasks[WHITE][pawnSq - 8] | PassedPawnMasks[BLACK][pawnSq]) & myKing)
  ) return true;

  if ((side2move == side) and
      (pawn & Rank3to5[side]) and
      (side == WHITE ? myKingR <= pawnR - 1 : myKingR >= pawnR + 1) and
      (PassedPawnMasks[side][pawnSq] & emyKing)
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

  const Bitboard occupied = pos.All();
  const Bitboard bishop   = pos.get_piece(side, BISHOP);
  const Square   bishopSq = SquareNo(bishop);

  if (pos.count<WHITE, ALL>() == 1)
  {
    // Pawn and bishop are of different side
    const Bitboard pawn = pos.get_piece(emySide, PAWN);
    const Square pawnSq = SquareNo(pawn);

    Bitboard pawnMask = plt::PassedPawnMasks[emySide][pawnSq] & plt::LineMasks[pawnSq];

    if ((bishop | pos.get_piece(side, KING)) & pawnMask)
      return true;

    if (AttackSquares<BISHOP>(bishopSq, 0) & pawnMask)
      return true;

    if (side2move == emySide)
      pawnMask ^= pawnMask & plt::PawnMasks[emySide][pawnSq];

    while (pawnMask)
    {
      const Square sq = NextSquare(pawnMask);

      const Bitboard bishopSquares = AttackSquares<BISHOP>(bishopSq, occupied) & AttackSquares<BISHOP>(sq, occupied);
      const Square   emyKingSq = SquareNo(pos.get_piece(emySide, KING));

      if (bishopSquares & ~plt::KingMasks[emyKingSq])
        return true;
    }

    return false;
  }

  const Bitboard pawn = pos.get_piece(side, PAWN);
  const Square pawnSq = SquareNo(pawn);

  Bitboard pawnMask = plt::PassedPawnMasks[side][pawnSq] & plt::LineMasks[pawnSq];
  Bitboard cornerMask = pawnMask & Rank18;

  if (((cornerMask & WhiteSquares) and (bishop & WhiteSquares))
   or ((cornerMask & BlackSquares) and (bishop & BlackSquares))) return false;

  if ((pawn & FileAH) and (pos.get_piece(emySide, KING) & pawnMask))
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

  const Bitboard emyKing = pos.get_piece(emySide, KING  );
  const Bitboard bishop  = pos.get_piece(emySide, BISHOP);

  const Square    kingSq = SquareNo(pos.get_piece(side, KING));
  const Square emyKingSq = SquareNo(emyKing);

  if ((AttackSquares<ROOK>(emyKingSq, 0) & bishop) == 0)
  {
    if (!(emyKing & (Rank18 | FileA | FileH)))
      return true;

    if (ChebyshevDistance(kingSq, emyKingSq) >= 4)
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

