

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

void
search(
  ChessBoard pos,
  Depth mDepth = MAX_DEPTH,
  double searchTime = DEFAULT_SEARCH_TIME,
  std::ostream& ostream = std::cout,
  bool debug = false,
  bool emitUciInfo = false
);

// PvNode is true when this node lies on the principal variation — the root,
// plus the first move searched at every PV node above it. Never a runtime
// value: a child is passed either its parent's PvNode or a literal `false`,
// so the distinction is resolved at compile time and the two node kinds
// specialize into separate functions. A PV node
// declines TT cutoffs so it always writes its pvArray row.
template <bool PvNode>
Score
alphaBeta(ChessBoard& pos, Depth depth, Score alpha, Score beta, Ply ply, int pvIndex, int numExtensions, bool doNull = true);


Score
rootAlphaBeta(ChessBoard& pos, Score alpha, Score beta, Depth depth);


#endif


