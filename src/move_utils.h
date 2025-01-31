
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
    constexpr int flagBit = mt << 21;

    while (endSquares > 0)
    {
      Square fp = NextSquare(endSquares);
      PieceType fpt = type_of(pos.PieceOnSquare(fp));

      Move move = baseMove | flagBit | (fpt << 15) | (fp << 6);
      movesArray.add(move);
    }
  }

  template<bool captures = true, bool quiet = true>
  MoveArray
  getMoves(const ChessBoard& pos) const noexcept
  {
    MoveArray myMoves;
    Color c = pos.color;
    int colorBit = c << 20;

    const auto addPawnMoves = [&] (int shift, Bitboard endSquares) {
      Move base_move = colorBit | (PAWN << 12);
      while (endSquares != 0)
      {
        Square fp = NextSquare(endSquares);
        Square ip = fp + shift;

        PieceType fpt = type_of(pos.PieceOnSquare(fp));

        Move move = base_move | (fpt << 15) | (fp << 6) | ip;
        myMoves.add(move);

        if (((1ULL << fp) & Rank18))
        {
          myMoves.add(move | 0xC0000);
          myMoves.add(move | 0x80000);
          myMoves.add(move | 0x40000);
        }
      }
    };

    // fix pawns
    Bitboard pawns = pos.get_piece(c, PAWN);
    Bitboard pawns2 = pawns & initSquares;
    Bitboard temp = initSquares ^ pawns2;

    if (checkers < 2)
    {
      if (captures)
      {
        addPawnMoves( 7 - 16 * c, pawnDestSquares[0]);
        addPawnMoves( 9 - 16 * c, pawnDestSquares[1]);
      }
      if (quiet)
      {
        addPawnMoves(16 - 32 * c, pawnDestSquares[2]);
        addPawnMoves( 8 - 16 * c, pawnDestSquares[3]);
      }

      while (pawns2 > 0)
      {
        Square ip = NextSquare(pawns2);
        PieceType ipt = type_of(pos.PieceOnSquare(ip));
        Bitboard finalSquares = destSquares[ip];
        
        while (finalSquares > 0)
        {
          Square fp = NextSquare(finalSquares);
          PieceType fpt = type_of(pos.PieceOnSquare(fp));

          Move move = colorBit | (fpt << 15) | (ipt << 12) | (fp << 6) | ip;
          myMoves.add(move);

          if ((ipt == PAWN) and ((1ULL << fp) & Rank18))
          {
            myMoves.add(move | 0xC0000);
            myMoves.add(move | 0x80000);
            myMoves.add(move | 0x40000);
          }
        }
      }
    }

    Bitboard epPawns = enpassantPawns;
    // Encode enpassant-pawns
    while (epPawns > 0)
    {
      Square ip = NextSquare(epPawns);
      Square fp = pos.EnPassantSquare();

      Move move = (CAPTURES << 21) | colorBit | (PAWN << 12) | (fp << 6) | ip; 
      myMoves.add(move);
    }

    while (temp > 0)
    {
      Square     ip = NextSquare(temp);
      PieceType ipt = type_of(pos.PieceOnSquare(ip));
      Move baseMove = colorBit | (ipt << 12) | ip;

      Bitboard finalSquares = destSquares[ip];
      Bitboard  captSquares = finalSquares & pos.get_piece(~c, ALL);
      Bitboard quietSquares = finalSquares ^ captSquares;

      if (captures)
        FillMoves<CAPTURES>(pos, myMoves, captSquares, baseMove);

      if (quiet)
        FillMoves<NORMAL>(pos, myMoves, quietSquares, baseMove);
    }
    return myMoves;
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


