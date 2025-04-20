
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
 * @brief Checks if the king of the active side is in check.
 * 
 * @param ChessBoard position
**/
template <Color cMy>
bool
inCheck(const ChessBoard& pos);

#endif


