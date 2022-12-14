

#ifndef SINGLE_THREAD_H
#define SINGLE_THREAD_H

#include "bitboard.h"
#include "movegen.h"
#include "search.h"
// #include "new_eval.h"


uint64_t
bulkcount(chessBoard &_cb, int depth);

int
negaMax(chessBoard &_cb, int depth);

std::pair<MoveType, int>
negaMax_root(chessBoard &_cb, int depth);

/**
 * @brief Iterative Search to search a board postion.
 * 
 * @param primary board position
 * @param mDepth maxDepth to which Iterartive search should to run
 * @param use_timer Run a time-based search.
 */
void
search_iterative(chessBoard primary, int mDepth = maxDepth, double search_time = default_search_time);

/**
 * @brief Returns the evaluation of a board at a given depth
 * 
 * @param board chessboard to evaluate
 * @param depth depth of the search
 * @param alpha 
 * @param beta 
 * @param ply distance from the root
 * @param pvIndex 
 * @return int 
 */
int
alphabeta(chessBoard &board, int depth, int alpha, int beta, int ply, int pvIndex);

int
pv_root_alphabeta(chessBoard &_cb, int alpha, int beta, int depth);

int
lmr_search(chessBoard &_cb, MoveList& myMoves, int depth, int alpha, int beta, int ply, int pvIndex);



#endif


