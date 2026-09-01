

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
 * @param pos board position
 * @param mDepth maxDepth to which Iterartive search should to run
 * @param searchTime time to run the search
 * @param ostream ostream to write the search results
 * @param debug debug mode
 */
void
search(
  ChessBoard pos,
  Depth mDepth = MAX_DEPTH,
  double searchTime = DEFAULT_SEARCH_TIME,
  std::ostream& ostream = std::cout,
  bool debug = false,
  bool emitUciInfo = false
);

/**
 * @brief Returns the evaluation of a board at a given depth
 *
 * @tparam PvNode true when this node lies on the principal variation — the
 *   root, plus the first move searched at every PV node above it. Never a
 *   runtime value: a child is passed either its parent's PvNode or a literal
 *   `false`, so the distinction is resolved at compile time and the two node
 *   kinds specialize into separate functions (same shape Stockfish uses).
 *   A PV node declines TT cutoffs so it always writes its pvArray row.
 * @param board ChessBoard to evaluate
 * @param depth depth of the search
 * @param alpha
 * @param beta
 * @param ply distance from the root
 * @param pvIndex
 * @return Score
 */
template <bool PvNode>
Score
alphaBeta(ChessBoard& pos, Depth depth, Score alpha, Score beta, Ply ply, int pvIndex, int numExtensions, bool doNull = true);


/**
 * @param perpMove a move proven (by perpetual.cpp) to force a draw from this
 *   root, or NULL_MOVE. A proof is a hard LOWER bound of VALUE_DRAW, so the
 *   root clamps alpha up to it and pre-seeds the PV with the move -- if no
 *   search line beats a draw, this is what gets played and reported.
 */
Score
rootAlphaBeta(ChessBoard& pos, Score alpha, Score beta, Depth depth, Move perpMove = NULL_MOVE);


#endif


