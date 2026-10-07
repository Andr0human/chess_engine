
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

  Color color;

  int checkers;

  Bitboard initSquares;

  Bitboard myPawns;

  Bitboard enpassantPawns;

  // One promotion that removeMove() took out. Holds from | to<<6 in bits 0..11
  // and the promotion piece in bits 18..19 (0=B, 1=N, 2=R, 3=Q, as in a Move).
  // 0 means nothing was removed, since from == to == 0 can't happen.
  Move promoSuppress;

  // How many moves removeMove() has taken out. playSubsetMoves adds it to
  // moveNo, so LMR's `moveNo < LMR_LIMIT` check counts the hash move that was
  // searched before the staged moves.
  uint16_t removedMovesCount;

  // Squares that give check to the enemy king, indexed by piece type:
  // {Pawn, Bishop, Knight, Rook, Queen}.
  Bitboard squaresThatCheckEnemyKing[5];

  // Initial squares from which a moving piece could potentially give a
  // discovered check to the opponent's king.
  Bitboard discoverCheckSquares;

  Bitboard pinnedPiecesSquares;

  Bitboard legalSquaresMaskInCheck;

  Bitboard myAttackedSquares;

  Bitboard enemyAttackedSquares;

  MoveList()
  : checkers(0), initSquares(0), enpassantPawns(0),
    promoSuppress(0), removedMovesCount(0), myAttackedSquares(0) {}

  MoveList(Color c)
  : color(c), checkers(0), initSquares(0), enpassantPawns(0),
    promoSuppress(0), removedMovesCount(0), myAttackedSquares(0) {}

  size_t
  removedMoves() const noexcept { return removedMovesCount; }

  void
  add(const ChessBoard& pos, Square sq, Bitboard _destSquares)
  {
    if (_destSquares == 0)
      return;
    destSquares[sq] = _destSquares;
    initSquares |= 1ULL << sq;
    myAttackedSquares |= _destSquares & pos.all();
  }

  void
  addPawns(const ChessBoard& pos, size_t index, Bitboard _destSquares)
  {
    pawnDestSquares[index] = _destSquares;
    myAttackedSquares |= _destSquares & pos.all();
  }

  size_t
  countMoves() const noexcept;

  // True if there is at least one legal move. Same as countMoves() != 0 but
  // stops at the first move. Used for the mate and stalemate test.
  bool
  anyMove() const noexcept;

  template<MType mt1=MType::CAPTURES | MType::QUIET, MType mt2=MType(0)>
  void
  getMoves(const ChessBoard& pos, MoveArray& movesArray) const noexcept;

  template<MType mt>
  bool
  exists(const ChessBoard& pos) const noexcept;

  // Remove one move so later getMoves<>() calls skip it. Used to drop the hash
  // move after it has been searched.
  //
  // The four promotions (Q/R/B/N) share one destSquares bit, so clearing the
  // bit would remove all four. Instead the move is saved in promoSuppress and
  // fillPawns skips just that one.
  void
  removeMove(Move m) noexcept;

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
