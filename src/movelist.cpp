
#include "attacks.h"
#include "movelist.h"

template <MType mt>
constexpr Move
MoveList::generateTypeBit() noexcept
{
  if constexpr (hasFlag(mt, MType::CAPTURES )) return Move(1) << 20;
  if constexpr (hasFlag(mt, MType::PROMOTION)) return Move(1) << 21;
  if constexpr (hasFlag(mt, MType::CHECK    )) return Move(1) << 23;

  return 0;
}

template <MType mt1, MType mt2>
void
MoveList::fillMoves(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr Move typeBit = generateTypeBit<mt1>();
  PieceType ipt = PieceType((baseMove >> 12) & 7);

  while (endSquares > 0)
  {
    Square fp = nextSquare(endSquares);
    Move move = baseMove | typeBit | (Move(fp) << 6);

    if (hasFlag(mt1, MType::CAPTURES))
      move |= Move(type_of(pos.pieceOnSquare(fp))) << 15;

    if (hasFlag(mt2, MType::CHECK) and ((1ULL << fp) & squaresThatCheckEnemyKing[ipt - 1]))
      move |= generateTypeBit<MType::CHECK>();

    movesArray.add(move);
  }
}

template <MType mt1, MType mt2>
void
MoveList::fillKingMoves(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr Move typeBit = generateTypeBit<mt1>();
  const Square ip = from_sq(baseMove);

  while (endSquares > 0)
  {
    Square fp = nextSquare(endSquares);
    Move move = baseMove | typeBit | (Move(fp) << 6);

    if (hasFlag(mt1, MType::CAPTURES))
      move |= Move(type_of(pos.pieceOnSquare(fp))) << 15;

    if (hasFlag(mt2, MType::CHECK)
      and ((1ULL << ip) & discoverCheckSquares)
      and ((1ULL << fp) & (~discoverCheckMasks[ip]))
    ) move |= generateTypeBit<MType::CHECK>();

    if (hasFlag(mt2, MType::CHECK) and (std::abs(fp - ip) == 2))
    {
      Bitboard rookBit =
        (fp > ip ? 32ULL : 8ULL) << (56 * (~color));

      if (rookBit & squaresThatCheckEnemyKing[ROOK - 1])
        move |= generateTypeBit<MType::CHECK>();
    }

    movesArray.add(move);
  }
}

template <MType mt1, MType mt2>
void
MoveList::fillEnpassantPawns(const ChessBoard& pos, MoveArray& movesArray) const noexcept
{
  const Move  typeBit = generateTypeBit<mt1>();
  const Move colorBit = Move(color) << 22;
  Bitboard epPawns = enpassantPawns;
  Square fp = pos.enPassantSquare();

  while (epPawns > 0)
  {
    Square ip = nextSquare(epPawns);
    Move move = typeBit | colorBit | (Move(PAWN) << 12) | (Move(fp) << 6) | ip;

    if (hasFlag(mt2, MType::CHECK))
    {
      if ((1ULL << fp) & squaresThatCheckEnemyKing[0])
        move |= generateTypeBit<MType::CHECK>();

      Square emyKingSq  = squareNo(pos.getPiece(~color, KING));
      Bitboard occupied = pos.all() ^ (1ULL << ip) ^ (1ULL << fp) ^ (1ULL << (fp - 8 * (2 * color - 1)));

      Bitboard attackBishop = attackSquares<BISHOP>(emyKingSq, occupied);
      Bitboard attackRook   = attackSquares<ROOK  >(emyKingSq, occupied);

      Bitboard rq = pos.getPiece(color, ROOK  ) | pos.getPiece(color, QUEEN);
      Bitboard bq = pos.getPiece(color, BISHOP) | pos.getPiece(color, QUEEN);

      if ((attackBishop & bq) or (attackRook & rq))
        move |= generateTypeBit<MType::CHECK>();
    }

    movesArray.add(move);
  }
}

template <MType mt1, MType mt2>
void
MoveList::fillShiftPawns(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  int shift
) const noexcept
{
  const Move colorBit = Move(color) << 22;
  const Move baseMove = generateTypeBit<mt1>() | colorBit | (Move(PAWN) << 12);

  while (endSquares > 0)
  {
    Square fp = nextSquare(endSquares);
    Square ip = fp + shift;

    Move move = baseMove | (Move(fp) << 6) | ip;

    if (hasFlag(mt2, MType::CHECK)
      and ((((1ULL << ip) & discoverCheckSquares) and ((1ULL << fp) & (~discoverCheckMasks[ip])))
       or ((1ULL << fp) & squaresThatCheckEnemyKing[0]))
    ) move |= generateTypeBit<MType::CHECK>();

    if (hasFlag(mt1, MType::CAPTURES))
      move |= Move(type_of(pos.pieceOnSquare(fp))) << 15;

    movesArray.add(move);
  }
}

template <MType mt1, MType mt2>
void
MoveList::fillPawns(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr Move typeBit = generateTypeBit<mt1>();
  const Square ip = from_sq(baseMove);

  while (endSquares > 0)
  {
    Square fp = nextSquare(endSquares);
    Move move = baseMove | typeBit | (Move(fp) << 6);
    Bitboard fPos = 1ULL << fp;

    if (hasFlag(mt1, MType::CAPTURES))
      move |= Move(type_of(pos.pieceOnSquare(fp))) << 15;

    if (fPos & Rank18)
    {
      move |= generateTypeBit<MType::PROMOTION>();
      Move moveQ = move | 0xC0000;
      Move moveR = move | 0x80000;
      Move moveN = move | 0x40000;
      Move moveB = move;

      if (hasFlag(mt2, MType::CHECK))
      {
        constexpr Move checkBit = generateTypeBit<MType::CHECK>();
        if (((1ULL << ip) & discoverCheckSquares) and (fPos & (~discoverCheckMasks[ip])))
        {
          moveQ |= checkBit;
          moveR |= checkBit;
          moveN |= checkBit;
          moveB |= checkBit;
        }
        else
        {
          Bitboard emyKing = pos.getPiece(~color, KING);
          Bitboard occupied = pos.all() ^ (1ULL << ip);

          Bitboard attackSqBishop = attackSquares<BISHOP>(fp, occupied);
          Bitboard attackSqRook   = attackSquares<ROOK  >(fp, occupied);
          Bitboard attackSqQueen  = attackSqBishop | attackSqRook;

          if (attackSqQueen  & emyKing) moveQ |= checkBit;
          if (attackSqRook   & emyKing) moveR |= checkBit;
          if (attackSqBishop & emyKing) moveB |= checkBit;
          if (fPos & squaresThatCheckEnemyKing[KNIGHT - 1]) moveN |= checkBit;
        }
      }

      movesArray.add(moveQ);
      movesArray.add(moveR);
      movesArray.add(moveN);
      movesArray.add(moveB);
    }
    else
    {
      if (hasFlag(mt2, MType::CHECK)
        and ((((1ULL << ip) & discoverCheckSquares) and (fPos & (~discoverCheckMasks[ip])))
         or (fPos & squaresThatCheckEnemyKing[0]))
      ) move |= generateTypeBit<MType::CHECK>();

      movesArray.add(move);
    }
  }
}

size_t
MoveList::countMoves() const noexcept
{
  Bitboard pawnMask  = myPawns & initSquares;
  Bitboard mask = initSquares ^ pawnMask;

  size_t moveCount = 0;

  if (checkers < 2)
  {
    moveCount += popCount(pawnDestSquares[0]) + popCount(pawnDestSquares[1])
               + popCount(pawnDestSquares[2]) + popCount(pawnDestSquares[3])
               + popCount(enpassantPawns);

    while (pawnMask)
    {
      Bitboard finalSquares = destSquares[nextSquare(pawnMask)];
      moveCount += 4 * popCount(finalSquares & Rank18) + popCount(finalSquares & ~Rank18);
    }
  }

  while (mask > 0)
    moveCount += popCount(destSquares[nextSquare(mask)]);

  return moveCount;
}

template<MType mt1, MType mt2>
void
MoveList::getMoves(const ChessBoard& pos, MoveArray& movesArray) const noexcept
{
  const Move colorBit = Move(color) << 22;

  // fix pawns
  Bitboard emyPieces = pos.getPiece(~color, ALL);
  Bitboard kingMask  = pos.getPiece(color, KING) & initSquares;
  Bitboard pawnMask  = myPawns & initSquares;
  Bitboard pieceMask = initSquares ^ (pawnMask | kingMask);

  if (checkers < 2)
  {
    if (hasFlag(mt1, MType::CAPTURES))
    {
      fillShiftPawns<MType::CAPTURES, mt2>(pos, movesArray, pawnDestSquares[0], 7 - 16 * color);
      fillShiftPawns<MType::CAPTURES, mt2>(pos, movesArray, pawnDestSquares[1], 9 - 16 * color);
    }
    if (hasFlag(mt1, MType::QUIET))
    {
      fillShiftPawns<MType::QUIET, mt2>(pos, movesArray, pawnDestSquares[2], 16 - 32 * color);
      fillShiftPawns<MType::QUIET, mt2>(pos, movesArray, pawnDestSquares[3],  8 - 16 * color);
    }

    while (pawnMask > 0)
    {
      Square ip = nextSquare(pawnMask);
      PieceType ipt = type_of(pos.pieceOnSquare(ip));
      Move baseMove = colorBit | (Move(ipt) << 12) | ip;

      Bitboard finalSquares = destSquares[ip];
      Bitboard  captSquares = finalSquares & emyPieces;
      Bitboard quietSquares = finalSquares ^ captSquares;

      if (hasFlag(mt1, MType::CAPTURES))
        fillPawns<MType::CAPTURES, mt2>(pos, movesArray,  captSquares, baseMove);

      if (hasFlag(mt1, MType::QUIET))
        fillPawns<MType::QUIET, mt2>(pos, movesArray, quietSquares, baseMove);
    }
  }

  if (hasFlag(mt1, MType::CAPTURES))
    fillEnpassantPawns<mt1, mt2>(pos, movesArray);

  while (pieceMask > 0)
  {
    Square     ip = nextSquare(pieceMask);
    PieceType ipt = type_of(pos.pieceOnSquare(ip));
    Move baseMove = colorBit | (Move(ipt) << 12) | Move(ip);

    if (hasFlag(mt2, MType::CHECK) and ((1ULL << ip) & discoverCheckSquares))
      baseMove |= generateTypeBit<MType::CHECK>();

    Bitboard finalSquares = destSquares[ip];
    Bitboard  captSquares = finalSquares & emyPieces;
    Bitboard quietSquares = finalSquares ^ captSquares;

    if (hasFlag(mt1, MType::CAPTURES))
      fillMoves<MType::CAPTURES, mt2>(pos, movesArray,  captSquares, baseMove);

    if (hasFlag(mt1, MType::QUIET))
      fillMoves<MType::QUIET, mt2>(pos, movesArray, quietSquares, baseMove);
  }

  if (kingMask)
  {
    Square     ip = squareNo(kingMask);
    Move baseMove = colorBit | (Move(KING) << 12) | ip;

    Bitboard finalSquares = destSquares[ip];
    Bitboard  captSquares = finalSquares & emyPieces;
    Bitboard quietSquares = finalSquares ^ captSquares;

    if (hasFlag(mt1, MType::CAPTURES))
      fillKingMoves<MType::CAPTURES, mt2>(pos, movesArray,  captSquares, baseMove);

    if (hasFlag(mt1, MType::QUIET))
      fillKingMoves<MType::QUIET, mt2>(pos, movesArray, quietSquares, baseMove);
  }
}

bool
MoveList::pawnCheckExists(Bitboard endSquares, int shift) const noexcept
{
  if (endSquares & squaresThatCheckEnemyKing[0]) return true;

  Bitboard ipMask = shift > 0 ? endSquares << shift : endSquares >> -shift;
  Bitboard mask = ipMask & discoverCheckSquares;

  while (mask)
  {
    Square ip = nextSquare(mask);
    Square fp = ip - shift;

    if ((1ULL << fp) & (~discoverCheckMasks[ip]))
      return true;
  }

  return false;
}

template <>
bool
MoveList::exists<MType::CAPTURES>(const ChessBoard& pos) const noexcept
{
  Bitboard emyPieces = pos.getPiece(~color, ALL);
  return (myAttackedSquares & emyPieces) or enpassantPawns;
}

template <>
bool
MoveList::exists<MType::CHECK>(const ChessBoard& pos) const noexcept
{
  Bitboard kingMask  = pos.getPiece(color, KING) & initSquares;
  Bitboard pawnMask  = myPawns & initSquares;
  Bitboard pieceMask = initSquares ^ (pawnMask | kingMask);

  if (checkers < 2)
  {
    while (pieceMask > 0)
    {
      Square     ip = nextSquare(pieceMask);
      PieceType ipt = type_of(pos.pieceOnSquare(ip));

      if (((1ULL << ip) & discoverCheckSquares) or (destSquares[ip] & squaresThatCheckEnemyKing[ipt - 1]))
        return true;
    }

    if (pawnCheckExists(pawnDestSquares[0],  7 - 16 * color)
     or pawnCheckExists(pawnDestSquares[1],  9 - 16 * color)
     or pawnCheckExists(pawnDestSquares[2], 16 - 32 * color)
     or pawnCheckExists(pawnDestSquares[3],  8 - 16 * color)
    ) return true;

    while (pawnMask > 0)
    {
      Square ip = nextSquare(pawnMask);
      Bitboard finalSquares = destSquares[ip];

      while (finalSquares > 0)
      {
        Square fp = nextSquare(finalSquares);
        Bitboard fPos = 1ULL << fp;

        if (fPos & Rank18)
        {
          if (((1ULL << ip) & discoverCheckSquares) and (fPos & (~discoverCheckMasks[ip])))
            return true;
          else
          {
            Bitboard emyKing = pos.getPiece(~color, KING);
            Bitboard occupied = pos.all() ^ (1ULL << ip);

            Bitboard attackSqBishop = attackSquares<BISHOP>(fp, occupied);
            Bitboard attackSqRook   = attackSquares<ROOK  >(fp, occupied);
            Bitboard attackSqQueen  = attackSqBishop | attackSqRook;

            if ((attackSqQueen  & emyKing)
             or (attackSqRook   & emyKing)
             or (attackSqBishop & emyKing)
             or (fPos & squaresThatCheckEnemyKing[KNIGHT - 1])
            ) return true;
          }
        }
        else
        {
          if (((((1ULL << ip) & discoverCheckSquares) and (fPos & ~discoverCheckMasks[ip]))
            or (fPos & squaresThatCheckEnemyKing[0]))
          ) return true;
        }
      }
    }

    if (enpassantPawns)
    {
      Square fp = pos.enPassantSquare();
      if ((1ULL << fp) & squaresThatCheckEnemyKing[0])
        return true;

      Bitboard epPawns = enpassantPawns;
      while (epPawns > 0)
      {
        Square ip = nextSquare(epPawns);
        Square emyKingSq  = squareNo(pos.getPiece(~color, KING));
        Bitboard occupied = pos.all() ^ (1ULL << ip) ^ (1ULL << fp) ^ (1ULL << (fp - 8 * (2 * color - 1)));

        Bitboard attackBishop = attackSquares<BISHOP>(emyKingSq, occupied);
        Bitboard attackRook   = attackSquares<ROOK  >(emyKingSq, occupied);

        Bitboard rq = pos.getPiece(color, ROOK  ) | pos.getPiece(color, QUEEN);
        Bitboard bq = pos.getPiece(color, BISHOP) | pos.getPiece(color, QUEEN);

        if ((attackBishop & bq) or (attackRook & rq))
          return true;
      }
    }
  }

  if (kingMask)
  {
    Square ip = squareNo(kingMask);
    Bitboard finalSquares = destSquares[ip];

    if (((1ULL << ip) & discoverCheckSquares) and (finalSquares & (~discoverCheckMasks[ip])))
      return true;

    while (finalSquares > 0)
    {
      Square fp = nextSquare(finalSquares);

      if (std::abs(fp - ip) == 2)
      {
        Bitboard rookBit = (fp > ip ? 32ULL : 8ULL) << (56 * (~color));
  
        if (rookBit & squaresThatCheckEnemyKing[ROOK - 1])
          return true;
      }
    }
  }

  return false;
}


template void MoveList::getMoves<MType(1), MType(0)>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<MType(1), MType(4)>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<MType(2), MType(0)>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<MType(2), MType(4)>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<MType(3), MType(0)>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<MType(3), MType(4)>(const ChessBoard&, MoveArray&) const noexcept;
