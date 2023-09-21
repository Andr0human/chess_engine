
#ifndef MOVE_UTILS_H
#define MOVE_UTILS_H

#include <iomanip>
#include "bitboard.h"
#include "lookup_table.h"


class MoveList
{
    // Note : Need to create a copy constructor before copying this class.
    // __begin, __end address pointer needs to fix for copying.
    private:
    Move* __begin;
    Move* __end;

    public:
    // Stores all moves for current position
    Move pMoves[MAX_MOVES];

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


    MoveList(Color c, bool qs = false)
    : __begin(pMoves), __end(pMoves), color(c), qsSearch(qs), checkers(0) {}

    inline void
    Add(Move move) noexcept
    { *__end++ = move; }

    inline bool
    empty() const noexcept
    { return __begin == __end; }

    inline Move*
    begin() const noexcept
    { return __begin; }

    inline Move*
    end() const noexcept
    { return __end; }

    inline uint64_t
    size() const noexcept
    { return __end - __begin; }
};


// // Returns and removes the LsbIndex
// inline Square
// NextSquare(uint64_t& __x)
// {
//     Square res = Square(__builtin_ctzll(__x));
//     __x &= __x - 1;
//     return res;
// }

// // Prints all the info on the encoded-move
// void
// DecodeMove(Move encoded_move);




/**
 * @brief Checks if the king of the active side is in check.
 * 
 * @param ChessBoard position
**/
// template <Color c_my>
// bool
// InCheck(const ChessBoard& _cb);


/**
 * @brief Returns string readable move from encoded-move.
 * Invalid move leads to undefined behaviour.
 * 
 * @param move encoded-move
 * @param _cb board position
 * @return string 
 */
// string
// PrintMove(Move move, ChessBoard _cb);


/**
 * @brief Prints all encoded-moves in list to human-readable strings
 * 
 * @param _cb board position
 * @param myMoves Movelist for board positions.
 */
// void
// PrintMovelist(MoveList myMoves, ChessBoard _cb);


#endif


