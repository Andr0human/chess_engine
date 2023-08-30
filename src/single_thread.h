

#ifndef SINGLE_THREAD_H
#define SINGLE_THREAD_H

#include "bitboard.h"
#include "movegen.h"
#include "search.h"


uint64_t
BulkCount(ChessBoard& pos, Depth depth);

Score
NegaMax(ChessBoard& pos, Depth depth);

std::pair<Move, Score>
RootNegaMax(ChessBoard& pos, Depth depth);

/**
 * @brief Iterative Search to search a board postion.
 * 
 * @param primary board position
 * @param mDepth maxDepth to which Iterartive search should to run
 * @param use_timer Run a time-based search.
 */
void
Search(ChessBoard primary, Depth mDepth = MAX_DEPTH, double search_time = DEFAULT_SEARCH_TIME, std::ostream& ostream = std::cout);

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
AlphaBeta(ChessBoard& pos, Depth depth, Score alpha, Score beta, int ply, int pvIndex, int numExtensions);

Score
RootAlphabeta(ChessBoard& pos, Score alpha, Score beta, Depth depth);

Score
LmrSearch(ChessBoard& pos, MoveList& myMoves, Depth depth, Score alpha, Score beta, int ply, int pvIndex, int numExtensions);



#endif


