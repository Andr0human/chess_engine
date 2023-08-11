

#ifndef MOVEGEN_H
#define MOVEGEN_H


#include "bitboard.h"
#include "lookup_table.h"
#include "move_utils.h"


bool is_passad_pawn(int idx, ChessBoard& _cb);
bool interesting_move(MoveType move, ChessBoard& _cb);
bool f_prune_move(MoveType move, ChessBoard& _cb);

// Checks if move is valid for given position
bool legal_move_for_position(int move, ChessBoard& pos);

// Returns true if board position has at least one legal move.
bool has_legal_moves(ChessBoard &_cb);

/**
 * @brief Returns a list of all the legal moves in current position.
 * 
 * @param _cb ChessBoard
 * @param qs_only 
 * @return MoveList 
 */
MoveList generate_moves(ChessBoard &_cb, bool qs_only = false);

#endif
