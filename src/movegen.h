

#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "bitboard.h"
#include "lookup_table.h"
#include "move_utils.h"


// Checks if move is valid for given position
bool IsLegalMoveForPosition(Move move, ChessBoard& pos);

// Returns true if board position has at least one legal move.
bool LegalMovesPresent(ChessBoard& _cb);

/**
 * @brief Returns a list of all the legal moves in current position.
 * 
 * @param _cb ChessBoard
 * @param qSearch 
 * @return MoveList 
 */
MoveList GenerateMoves(ChessBoard& pos, bool qsSearch = false, bool findChecks = false);


// Prints all the info on the encoded-move
void
DecodeMove(Move encoded_move);

template <Color c_my>
bool
InCheck(const ChessBoard& _cb);


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
