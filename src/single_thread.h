

#ifndef SINGLE_THREAD_H
#define SINGLE_THREAD_H

#include "bitboard.h"
#include "movegen.h"
#include "search.h"
#include "evaluation.h"
#include "endgame.h"
#include "node_state.h"


typedef int (*ReductionFunc)(Depth depth, size_t move_no);


uint64_t
bulkCount(ChessBoard& pos, Depth depth);

void
search(
  ChessBoard pos,
  Depth mDepth = MAX_DEPTH,
  double searchTime = DEFAULT_SEARCH_TIME,
  std::ostream& ostream = std::cout,
  bool debug = false,
  bool emitUciInfo = false
);

// PvNode is true for nodes on the principal variation: the root, and the first
// move searched at each PV node. It's a template parameter, not a runtime
// value, because a child always gets its parent's PvNode or `false`, so the
// two kinds of node compile to separate functions. A PV node doesn't take TT
// cutoffs, so it always writes its pvArray row.
template <bool PvNode>
Score
alphaBeta(ChessBoard& pos, SearchContext ctx);


Score
rootAlphaBeta(ChessBoard& pos, Score alpha, Score beta, Depth depth);


#endif


