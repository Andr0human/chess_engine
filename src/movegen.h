

#ifndef MOVEGEN_H
#define MOVEGEN_H


#include "bitboard.h"
#include "lookup_table.h"
#include "move_utils.h"


bool IsPassedPawn(Square idx, ChessBoard& _cb);
bool InterestingMove(Move move, ChessBoard& _cb);
bool FutilityPruneMove(Move move, ChessBoard& _cb);
int SearchExtension(const ChessBoard& pos, int numExtensions);

// Checks if move is valid for given position
bool IsLegalMoveForPosition(Move move, ChessBoard& pos);

// Returns true if board position has at least one legal move.
bool LegalMovesPresent(ChessBoard &_cb);

/**
 * @brief Returns a list of all the legal moves in current position.
 * 
 * @param _cb ChessBoard
 * @param qSearch 
 * @return MoveList 
 */
MoveList GenerateMoves(ChessBoard& pos, bool qsSearch = false);

#endif
