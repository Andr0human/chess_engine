
#ifndef MOVE_UTILS_H
#define MOVE_UTILS_H

#include <array>
#include <iomanip>
#include "bitboard.h"
#include "lookup_table.h"


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


/**
 * @brief Prints all encoded-moves in list to human-readable strings
 * 
 * @param _cb board position
 * @param myMoves Movelist for board positions.
 */
void
PrintMovelist(MoveList myMoves, ChessBoard _cb);


#endif


