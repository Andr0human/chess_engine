
#ifndef MOVE_UTILS_H
#define MOVE_UTILS_H

#include <array>
#include <iomanip>
#include "attacks.h"
#include "bitboard.h"
#include "lookup_table.h"
#include "types.h"
#include "varray.h"


using MoveArray = Varray<Move, MAX_MOVES>;


class MoveList
{
  public:

  array<Bitboard, 4> pawnDestSquares;
  array<Bitboard , SQUARE_NB>  destSquares;

  // Active side color
  Color color;

  int checkers;

  Bitboard initSquares;

  Bitboard enpassantPawns;

  // Array to store squares that give check to the enemy king
  // {Pawn, Bishop, Knight, Rook, Queen}
  // Index 1: Squares where a bishop can give check to the enemy king
  Bitboard squaresThatCheckEnemyKing[5];

  // Bitboard of initial squares from which a moving piece
  // could potentially give a discovered check to the opponent's king.
  Bitboard discoverCheckSquares;

  // Bitboard of squares that have pinned pieces on them
  Bitboard pinnedPiecesSquares;

  // Bitboard representing squares under enemy attack
  Bitboard enemyAttackedSquares;

  MoveList(Color c)
  : color(c), checkers(0), initSquares(0), enpassantPawns(0) {}

  void
  Add(Square sq, Bitboard _destSquares)
  {
    if (_destSquares == 0)
      return;
    destSquares[sq] = _destSquares;
    initSquares |= 1ULL << sq;
  }

  void
  AddPawns(size_t index, Bitboard _destSquares)
  { pawnDestSquares[index] = _destSquares; }

  template <MoveType mt>
  void
  FillMoves(
    const ChessBoard& pos,
    MoveArray& movesArray,
    Bitboard endSquares,
    Move baseMove
  ) const noexcept
  {
    constexpr int typeBit = mt << 21;

    while (endSquares > 0)
    {
      Square fp = NextSquare(endSquares);
      PieceType fpt = type_of(pos.PieceOnSquare(fp));

      Move move = baseMove | typeBit | (fpt << 15) | (fp << 6);
      movesArray.add(move);
    }
  }

  void
  FillEnpassantPawns(MoveArray& movesArray, Square fp) const noexcept
  {
    constexpr Move typeBit = CAPTURES << 21;
    const int colorBit = color << 20;
    Bitboard epPawns = enpassantPawns;

    while (epPawns > 0)
    {
      Square ip = NextSquare(epPawns);

      Move move = typeBit | (colorBit) | (PAWN << 12) | (fp << 6) | ip;
      movesArray.add(move);
    }
  }

  template <MoveType mt>
  void
  FillShiftPawns(
    const ChessBoard& pos,
    MoveArray& movesArray,
    Bitboard endSquares,
    int shift
  ) const noexcept
  {
    const int  colorBit = color << 20;
    const Move baseMove = (mt << 21) | colorBit | (PAWN << 12);

    while (endSquares > 0)
    {
      Square fp = NextSquare(endSquares);
      Square ip = fp + shift;

      PieceType fpt = type_of(pos.PieceOnSquare(fp));

      Move move = baseMove | (fpt << 15) | (fp << 6) | ip;
      movesArray.add(move);

      if (((1ULL << fp) & Rank18))
      {
        movesArray.add(move | 0xC0000);
        movesArray.add(move | 0x80000);
        movesArray.add(move | 0x40000);
      }
    }
  }

  template <MoveType mt>
  void
  FillPawns(
    const ChessBoard& pos,
    MoveArray& movesArray,
    Bitboard endSquares,
    Move baseMove
  ) const noexcept
  {
    constexpr int typeBit = mt << 21;
    while (endSquares > 0)
    {
      Square fp = NextSquare(endSquares);
      PieceType fpt = type_of(pos.PieceOnSquare(fp));

      Move move = baseMove | typeBit | (fpt << 15) | (fp << 6);
      movesArray.add(move);

      if ((1ULL << fp) & Rank18)
      {
        movesArray.add(move | 0xC0000);
        movesArray.add(move | 0x80000);
        movesArray.add(move | 0x40000);
      }
    }
  }

  template<bool captures=true, bool quiet=true>
  void
  getMoves(const ChessBoard& pos, MoveArray& myMoves) const noexcept
  {
    const int colorBit = color << 21;

    // fix pawns
    Bitboard emyPieces = pos.get_piece(~color, ALL);
    Bitboard myPawns   = pos.get_piece(color, PAWN);
    Bitboard pawnMask  = myPawns & initSquares;
    Bitboard pieceMask = initSquares ^ pawnMask;

    if (checkers < 2)
    {
      if (captures)
      {
        FillShiftPawns<CAPTURES>(pos, myMoves, pawnDestSquares[0], 7 - 16 * color);
        FillShiftPawns<CAPTURES>(pos, myMoves, pawnDestSquares[1], 9 - 16 * color);
      }
      if (quiet)
      {
        FillShiftPawns<NORMAL>(pos, myMoves, pawnDestSquares[2], 16 - 32 * color);
        FillShiftPawns<NORMAL>(pos, myMoves, pawnDestSquares[3],  8 - 16 * color);
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
          FillPawns<CAPTURES>(pos, myMoves,  captSquares, baseMove);

        if (quiet)
          FillPawns<NORMAL  >(pos, myMoves, quietSquares, baseMove);
      }
    }

    if (captures)
      FillEnpassantPawns(myMoves, pos.EnPassantSquare());

    while (pieceMask > 0)
    {
      Square     ip = NextSquare(pieceMask);
      PieceType ipt = type_of(pos.PieceOnSquare(ip));
      Move baseMove = colorBit | (ipt << 12) | ip;

      Bitboard finalSquares = destSquares[ip];
      Bitboard  captSquares = finalSquares & emyPieces;
      Bitboard quietSquares = finalSquares ^ captSquares;

      if (captures)
        FillMoves<CAPTURES>(pos, myMoves, captSquares, baseMove);

      if (quiet)
        FillMoves<NORMAL  >(pos, myMoves, quietSquares, baseMove);
    }
  }
};


// Prints all the info on the encoded-move
void
DecodeMove(Move encoded_move);


/**
 * @brief Returns string readable move from encoded-move.
 * Invalid move leads to undefined behaviour.
 *
 * @param move encoded-move
 * @param _cb board position
 * @return string
 */
string
PrintMove(Move move, ChessBoard _cb);


#endif


