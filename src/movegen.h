

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

#endif
