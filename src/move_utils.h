
#ifndef MOVE_UTILS_H
#define MOVE_UTILS_H

#include "bitboard.h"


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
 * @brief Checks if the king of the active side is in check.
 * 
 * @param ChessBoard position
**/
template <Color c_my>
bool
InCheck(const ChessBoard& _cb);

#endif


