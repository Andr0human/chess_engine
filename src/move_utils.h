
#ifndef MOVE_UTILS_H
#define MOVE_UTILS_H

#include "bitboard.h"


// Prints all the info on the encoded-move
void
decodeMove(Move encodedMove);

/**
 * @brief Returns string readable move from encoded-move.
 * Invalid move leads to undefined behaviour.
 *
 * @param move encoded-move
 * @param pos board position
 * @return string
 */
string
printMove(Move move, ChessBoard pos);

/**
 * @brief Returns the UCI long-algebraic string for an encoded move
 * (e.g. "e2e4", "e7e8q", "e1g1" for kingside castle).
 */
string
moveToUci(Move move);

/**
 * @brief Parses a UCI long-algebraic string and returns the matching
 * legal encoded-move for the given position. Returns NULL_MOVE if the
 * string does not correspond to any legal move.
 */
Move
moveFromUci(const string& uci, const ChessBoard& pos);

/**
 * @brief Checks if the king of the active side is in check.
 * 
 * @param ChessBoard position
**/
template <Color cMy>
bool
inCheck(const ChessBoard& pos);

#endif


