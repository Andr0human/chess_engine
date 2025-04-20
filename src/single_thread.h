

#ifndef SINGLE_THREAD_H
#define SINGLE_THREAD_H

#include "bitboard.h"
#include "movegen.h"
#include "search.h"
#include "evaluation.h"
#include "endgame.h"


typedef int (*ReductionFunc)(Depth depth, size_t move_no);


uint64_t
bulkCount(ChessBoard& pos, Depth depth);

/**
 * @brief Iterative Search to search a board postion.
 * 
 * @param primary board position
 * @param mDepth maxDepth to which Iterartive search should to run
 * @param use_timer Run a time-based search.
 */
void
search(
  ChessBoard primary,
  Depth mDepth = MAX_DEPTH,
  double searchTime = DEFAULT_SEARCH_TIME,
  std::ostream& ostream = std::cout
);

/**
 * @brief Returns the evaluation of a board at a given depth
 * 
 * @param board ChessBoard to evaluate
 * @param depth depth of the search
 * @param alpha 
 * @param beta 
 * @param ply distance from the root
 * @param pvIndex 
 * @return Score 
 */
Score
alphaBeta(ChessBoard& pos, Depth depth, Score alpha, Score beta, Ply ply, int pvIndex, int numExtensions);


Score
rootAlphaBeta(ChessBoard& pos, Score alpha, Score beta, Depth depth);


#endif


