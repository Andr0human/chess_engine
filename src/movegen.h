

#ifndef MOVEGEN_H
#define MOVEGEN_H


#include "bitboard.h"
#include "lookup_table.h"
#include "move_utils.h"


bool is_passad_pawn(int idx, chessBoard& _cb);
bool interesting_move(MoveType move, chessBoard& _cb);
bool f_prune_move(MoveType move, chessBoard& _cb);


// Returns true if board position has at least one legal move.
bool has_legal_moves(chessBoard &_cb);

/**
 * @brief Returns a list of all the legal moves in current position.
 * 
 * @param _cb chessBoard
 * @param qs_only 
 * @return MoveList 
 */
MoveList generate_moves(chessBoard &_cb, bool qs_only = false);

#endif
