
#ifndef MOVE_LIST_H
#define MOVE_LIST_H

#include "varray.h"
#include "bitboard.h"

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

  // Bitboard representing squares current side is attacking
  Bitboard myAttackedSquares;

  // Bitboard representing squares under enemy attack
  Bitboard enemyAttackedSquares;

  MoveList(Color c)
  : color(c), checkers(0), initSquares(0), enpassantPawns(0) {}

  void
  add(Square sq, Bitboard _destSquares)
  {
    if (_destSquares == 0)
      return;
    destSquares[sq] = _destSquares;
    initSquares |= 1ULL << sq;
  }

  void
  addPawns(size_t index, Bitboard _destSquares)
  { pawnDestSquares[index] = _destSquares; }

  size_t
  countMoves() const noexcept;

  template<MType mt1=MType::CAPTURES | MType::QUIET, MType mt2=MType(0)>
  void
  getMoves(const ChessBoard& pos, MoveArray& movesArray) const noexcept;

  template<MType mt>
  bool
  exists(const ChessBoard& pos) const noexcept;

private:
  template <MType mt1, MType mt2>
  void
  fillMoves(
    const ChessBoard& pos,
    MoveArray& movesArray,
    Bitboard endSquares,
    Move baseMove
  ) const noexcept;

  template <MType mt1, MType mt2>
  void
  fillEnpassantPawns(const ChessBoard& pos, MoveArray& movesArray) const noexcept;

  template <MType mt1, MType mt2>
  void
  fillShiftPawns(
    const ChessBoard& pos,
    MoveArray& movesArray,
    Bitboard endSquares,
    int shift
  ) const noexcept;

  template <MType mt1, MType mt2>
  void
  fillPawns(
    const ChessBoard& pos,
    MoveArray& movesArray,
    Bitboard endSquares,
    Move baseMove
  ) const noexcept;

  template <MType mt1, MType mt2>
  void
  fillKingMoves(
    const ChessBoard& pos,
    MoveArray& movesArray,
    Bitboard endSquares,
    Move baseMove
  ) const noexcept;

  template <MType mt>
  static constexpr Move
  generateTypeBit() noexcept;

  bool
  pawnCheckExists(Bitboard endSquares, int shift) const noexcept;
};

#endif
