
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
  private:
  size_t moveCount;

  public:
  // Stores all moves for current position
  array<Move, MAX_MOVES> pMoves;

  // Active side color
  Color color;

  // Generate moves for Quisense Search
  bool qsSearch;

  int checkers;

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

  MoveList(Color c, bool qs = false)
  : moveCount(0), color(c), qsSearch(qs), checkers(0) {}

  inline void
  Add(Move move) noexcept
  { pMoves[moveCount++] = move; }

  inline bool
  empty() const noexcept
  { return moveCount == 0; }

  Move*
  begin() noexcept
  { return pMoves.begin(); }

  Move*
  end() noexcept
  { return pMoves.begin() + moveCount; }

  const Move*
  begin() const noexcept
  { return pMoves.begin(); }

  const Move*
  end() const noexcept
  { return pMoves.begin() + moveCount; }

  inline uint64_t
  size() const noexcept
  { return moveCount; }
};


class MoveList2
{
  public:

  array<Bitboard, 4> pawnDestSquares;
  array<Bitboard , SQUARE_NB>  destSquares;

  // Active side color
  Color color;

  // Generate moves for Quisense Search
  bool qsSearch;

  int checkers;

  Bitboard initSquares;

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

  MoveList2(Color c, bool qs = false)
  : color(c), qsSearch(qs), checkers(0), initSquares(0) {}

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

    if (checkers < 2)
    {
      addPawnMoves( 7 - 16 * c, pawnDestSquares[0]);
      addPawnMoves( 9 - 16 * c, pawnDestSquares[1]);
      addPawnMoves(16 - 32 * c, pawnDestSquares[2]);
      addPawnMoves( 8 - 16 * c, pawnDestSquares[3]);
    }

    uint64_t temp = initSquares;

    while (temp > 0)
    {
      Square ip = NextSquare(temp);
      Bitboard finalSquares = destSquares[ip];
      
      while (finalSquares > 0)
      {
        Square fp = NextSquare(finalSquares);

        PieceType ipt = type_of(pos.PieceOnSquare(ip));
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


