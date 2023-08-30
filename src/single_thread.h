

#ifndef SINGLE_THREAD_H
#define SINGLE_THREAD_H

#include "bitboard.h"
#include "movegen.h"
#include "search.h"


uint64_t
bulkcount(ChessBoard& pos, Depth depth);

Score
negaMax(ChessBoard& pos, Depth depth);

std::pair<Move, Score>
negaMax_root(ChessBoard& pos, Depth depth);

/**
 * @brief Iterative Search to search a board postion.
 * 
 * @param primary board position
 * @param mDepth maxDepth to which Iterartive search should to run
 * @param use_timer Run a time-based search.
 */
void
search_iterative(ChessBoard primary, Depth mDepth = MAX_DEPTH, double search_time = DEFAULT_SEARCH_TIME, std::ostream& ostream = std::cout);

/**
 * @brief Returns the evaluation of a board at a given depth
 * 
 * @param board ChessBoard to evaluate
 * @param depth depth of the search
 * @param alpha 
 * @param beta 
 * @param ply distance from the root
 * @param pvIndex 
 * @return int 
 */
Score
alphabeta(ChessBoard& pos, Depth depth, Score alpha, Score beta, int ply, int pvIndex, int numExtensions);

Score
pv_root_alphabeta(ChessBoard& pos, Score alpha, Score beta, Depth depth);

Score
lmr_search(ChessBoard& pos, MoveList& myMoves, Depth depth, Score alpha, Score beta, int ply, int pvIndex, int numExtensions);



#endif


