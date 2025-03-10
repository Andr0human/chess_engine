
#include "attacks.h"
#include "movelist.h"

template <MoveType mt, bool checks>
void
MoveList::FillMoves(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr int typeBit = mt << 20;
  PieceType ipt = PieceType((baseMove >> 12) & 7);

  while (endSquares > 0)
  {
    Square fp = NextSquare(endSquares);
    Move move = baseMove | typeBit | (fp << 6);

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    if (checks and ((1ULL << fp) & squaresThatCheckEnemyKing[ipt - 1]))
      move |= 1 << 23;

    movesArray.add(move);
  }
}

template <MoveType mt, bool checks>
void
MoveList::FillKingMoves(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr int typeBit = mt << 20;
  const Square ip = from_sq(baseMove);

  while (endSquares > 0)
  {
    Square fp = NextSquare(endSquares);
    Move move = baseMove | typeBit | (fp << 6);

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    if (checks and ((1ULL << ip) & discoverCheckSquares) and ((1ULL << fp) & (~discoverCheckMasks[ip])))
      move |= 1 << 23;

    if (checks and (std::abs(fp - ip) == 2))
    {
      Bitboard rookBit =
        (fp > ip ? 32ULL : 8ULL) << (56 * (~color));

      if (rookBit & squaresThatCheckEnemyKing[ROOK - 1])
        move |= 1 << 23;
    }

    movesArray.add(move);
  }
}

template <bool checks>
void
MoveList::FillEnpassantPawns(const ChessBoard& pos, MoveArray& movesArray) const noexcept
{
  constexpr Move typeBit = CAPTURES << 20;
  const int colorBit = color << 22;
  Bitboard epPawns = enpassantPawns;
  Square fp = pos.EnPassantSquare();

  while (epPawns > 0)
  {
    Square ip = NextSquare(epPawns);
    Move move = typeBit | (colorBit) | (PAWN << 12) | (fp << 6) | ip;

    if (checks)
    {
      if ((1ULL << fp) & squaresThatCheckEnemyKing[0])
        move |= 1 << 23;

      Square emyKingSq  = SquareNo(pos.get_piece(~color, KING));
      Bitboard occupied = pos.All() ^ (1ULL << ip) ^ (1ULL << fp) ^ (1ULL << (fp - 8 * (2 * color - 1)));

      Bitboard attackBishop = AttackSquares<BISHOP>(emyKingSq, occupied);
      Bitboard attackRook   = AttackSquares<ROOK  >(emyKingSq, occupied);

      Bitboard rq = pos.get_piece(color, ROOK  ) | pos.get_piece(color, QUEEN);
      Bitboard bq = pos.get_piece(color, BISHOP) | pos.get_piece(color, QUEEN);

      if ((attackBishop & bq) or (attackRook & rq))
        move |= 1 << 23;
    }

    movesArray.add(move);
  }
}

template <MoveType mt, bool checks>
void
MoveList::FillShiftPawns(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  int shift
) const noexcept
{
  const int  colorBit = color << 22;
  const Move baseMove = (mt << 20) | colorBit | (PAWN << 12);

  while (endSquares > 0)
  {
    Square fp = NextSquare(endSquares);
    Square ip = fp + shift;

    Move move = baseMove | (fp << 6) | ip;

    if (checks
      and ((((1ULL << ip) & discoverCheckSquares) and ((1ULL << fp) & (~discoverCheckMasks[ip])))
       or ((1ULL << fp) & squaresThatCheckEnemyKing[0]))
    ) move |= 1 << 23;

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    movesArray.add(move);
  }
}

template <MoveType mt, bool checks>
void
MoveList::FillPawns(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr int typeBit  = mt << 20;
  constexpr int checkBit = 1 << 23;
  const Square ip = from_sq(baseMove);

  while (endSquares > 0)
  {
    Square fp = NextSquare(endSquares);
    Move move = baseMove | typeBit | (fp << 6);
    Bitboard fPos = 1ULL << fp;

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    if (fPos & Rank18)
    {
      move |= PROMOTION << 20;
      Move moveQ = move | 0xC0000;
      Move moveR = move | 0x80000;
      Move moveN = move | 0x40000;
      Move moveB = move;

      if (checks)
      {
        if (((1ULL << ip) & discoverCheckSquares) and (fPos & (~discoverCheckMasks[ip])))
        {
          moveQ |= checkBit;
          moveR |= checkBit;
          moveN |= checkBit;
          moveB |= checkBit;
        }
        else
        {
          Bitboard emyKing = pos.get_piece(~color, KING);
          Bitboard occupied = pos.All() ^ (1ULL << ip);

          Bitboard attackSqBishop = AttackSquares<BISHOP>(fp, occupied);
          Bitboard attackSqRook   = AttackSquares<ROOK  >(fp, occupied);
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
      if (checks
        and ((((1ULL << ip) & discoverCheckSquares) and (fPos & (~discoverCheckMasks[ip])))
         or (fPos & squaresThatCheckEnemyKing[0]))
      ) move |= 1 << 23;

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
    moveCount += PopCount(pawnDestSquares[0]) + PopCount(pawnDestSquares[1])
               + PopCount(pawnDestSquares[2]) + PopCount(pawnDestSquares[3])
               + PopCount(enpassantPawns);

    while (pawnMask)
    {
      Bitboard finalSquares = destSquares[NextSquare(pawnMask)];
      moveCount += 4 * PopCount(finalSquares & Rank18) + PopCount(finalSquares & ~Rank18);
    }
  }

  while (mask > 0)
    moveCount += PopCount(destSquares[NextSquare(mask)]);

  return moveCount;
}

template<bool captures, bool quiet, bool checks>
void
MoveList::getMoves(const ChessBoard& pos, MoveArray& myMoves) const noexcept
{
  const int colorBit = color << 22;

  // fix pawns
  Bitboard emyPieces = pos.get_piece(~color, ALL);
  Bitboard kingMask  = pos.get_piece(color, KING) & initSquares;
  Bitboard pawnMask  = myPawns & initSquares;
  Bitboard pieceMask = initSquares ^ (pawnMask | kingMask);

  if (checkers < 2)
  {
    if (captures)
    {
      FillShiftPawns<CAPTURES, checks>(pos, myMoves, pawnDestSquares[0], 7 - 16 * color);
      FillShiftPawns<CAPTURES, checks>(pos, myMoves, pawnDestSquares[1], 9 - 16 * color);
    }
    if (quiet)
    {
      FillShiftPawns<NORMAL, checks>(pos, myMoves, pawnDestSquares[2], 16 - 32 * color);
      FillShiftPawns<NORMAL, checks>(pos, myMoves, pawnDestSquares[3],  8 - 16 * color);
    }

    while (pawnMask > 0)
    {
      Square ip = NextSquare(pawnMask);
      PieceType ipt = type_of(pos.PieceOnSquare(ip));
      Move baseMove = colorBit | (ipt << 12) | ip;

      Bitboard finalSquares = destSquares[ip];
      Bitboard  captSquares = finalSquares & emyPieces;
      Bitboard quietSquares = finalSquares ^ captSquares;

      if (captures)
        FillPawns<CAPTURES, checks>(pos, myMoves,  captSquares, baseMove);

      if (quiet)
        FillPawns<NORMAL  , checks>(pos, myMoves, quietSquares, baseMove);
    }
  }

  if (captures)
    FillEnpassantPawns<checks>(pos, myMoves);

  while (pieceMask > 0)
  {
    Square     ip = NextSquare(pieceMask);
    PieceType ipt = type_of(pos.PieceOnSquare(ip));
    Move baseMove = colorBit | (ipt << 12) | ip;

    if (checks and ((1ULL << ip) & discoverCheckSquares))
      baseMove |= 1 << 23;

    Bitboard finalSquares = destSquares[ip];
    Bitboard  captSquares = finalSquares & emyPieces;
    Bitboard quietSquares = finalSquares ^ captSquares;

    if (captures)
      FillMoves<CAPTURES, checks>(pos, myMoves, captSquares, baseMove);

    if (quiet)
      FillMoves<NORMAL  , checks>(pos, myMoves, quietSquares, baseMove);
  }

  if (kingMask)
  {
    Square     ip = SquareNo(kingMask);
    Move baseMove = colorBit | (KING << 12) | ip;

    Bitboard finalSquares = destSquares[ip];
    Bitboard  captSquares = finalSquares & emyPieces;
    Bitboard quietSquares = finalSquares ^ captSquares;

    if (captures)
      FillKingMoves<CAPTURES, checks>(pos, myMoves, captSquares, baseMove);

    if (quiet)
      FillKingMoves<NORMAL  , checks>(pos, myMoves, quietSquares, baseMove);
  }
}


template void MoveList::getMoves<true , true , true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<true , false, true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, true , true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, false, true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<true , true , false>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<true , false, false>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, true , false>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, false, false>(const ChessBoard&, MoveArray&) const noexcept;
