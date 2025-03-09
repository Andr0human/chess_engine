
#ifndef MOVE_UTILS_H
#define MOVE_UTILS_H

#include <iomanip>
#include "attacks.h"
#include "bitboard.h"
#include "varray.h"


using MoveArray = Varray<Move, MAX_MOVES>;


class MoveList
{
  public:

  array<Bitboard, 4> pawnDestSquares;
  array<Bitboard, SQUARE_NB> destSquares;
  array<Bitboard, SQUARE_NB> discoverCheckMasks;

  // Active side color
  Color color;

  int checkers;

  Bitboard initSquares;

  Bitboard myPawns;

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

  // Bitboard representing squares that the pieces of active
  // side can legally move to when the king is under check
  Bitboard legalSquaresMaskInCheck;

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

  size_t
  countMoves() const noexcept;

  template<bool captures=true, bool quiet=true, bool checks=false>
  void
  getMoves(const ChessBoard& pos, MoveArray& myMoves) const noexcept;

  private:

  template <MoveType mt, bool checks, NextSquareFunc nextSquare>
  void
  FillMoves(
    const ChessBoard& pos,
    MoveArray& movesArray,
    Bitboard endSquares,
    Move baseMove
  ) const noexcept;

  template <bool checks, NextSquareFunc nextSquare>
  void
  FillEnpassantPawns(const ChessBoard& pos, MoveArray& movesArray) const noexcept;

  template <MoveType mt, bool checks, NextSquareFunc nextSquare>
  void
  FillShiftPawns(
    const ChessBoard& pos,
    MoveArray& movesArray,
    Bitboard endSquares,
    int shift
  ) const noexcept;

  template <MoveType mt, bool checks, NextSquareFunc nextSquare>
  void
  FillPawns(
    const ChessBoard& pos,
    MoveArray& movesArray,
    Bitboard endSquares,
    Move baseMove
  ) const noexcept;

  template <MoveType mt, bool checks, NextSquareFunc nextSquare>
  void
  FillKingMoves(
    const ChessBoard& pos,
    MoveArray& movesArray,
    Bitboard endSquares,
    Move baseMove
  ) const noexcept;

  template<bool captures, bool quiet, bool checks, NextSquareFunc nextSquare>
  void
  getMovesImpl(const ChessBoard& pos, MoveArray& myMoves) const noexcept;
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


