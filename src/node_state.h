
#ifndef NODE_STATE_H
#define NODE_STATE_H

#include "types.h"
#include <optional>


/**
 * Per-node search state shared across playAllMoves / playSubsetMoves /
 * playMove inside the alpha-beta recursion. `alpha` and `hashf` mutate as
 * moves improve the bound / trigger cutoffs; the rest are constant within
 * a node.
 */
struct NodeState
{
  Score alpha;
  Score beta;
  Depth depth;
  Ply ply;
  int pvIndex;
  int numExtensions;
  Flag hashf = Flag::HASH_ALPHA;

  // Node static eval, computed at most once per node (lazy) and reused across
  // RFP / razoring / futility / improving. nullopt until first requested.
  std::optional<Score> staticEval = std::nullopt;

  constexpr int pvNextIndex() const noexcept { return pvIndex + MAX_PLY - ply; }
};


/**
 * Outcome of the TT-hash-move fast path inside alphaBeta.
 *   searched = true  -> the move was legal and searched; caller must drop it
 *                       from myMoves before staged movegen re-emits it.
 *   result.has_value -> caller should propagate this value out of alphaBeta
 *                       (timeout, or beta cutoff already TT-recorded inside).
 */
struct HashMoveOutcome
{
  bool searched = false;
  std::optional<Score> result;
};


#endif
