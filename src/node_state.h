
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

  // NOTE: PV-ness is deliberately *not* stored here. It is a compile-time
  // property (`template <bool PvNode>` on alphaBeta and the play* helpers),
  // since a node is only ever handed its parent's PvNode or a literal false.

  Flag hashf = Flag::HASH_ALPHA;

  // Node static eval, computed at most once per node (lazy) and reused across
  // RFP / razoring / futility / improving. nullopt until first requested.
  std::optional<Score> staticEval = std::nullopt;

  // Quiet-move futility flag, set in alphaBeta when the node's static eval sits
  // a depth-scaled margin below alpha. Consumed in the QUIET stage of
  // playSubsetMoves to skip the residual quiet moves once one move is searched.
  bool quietFutile = false;

  // Set when playSubsetMoves bails out mid-node on shouldStop(). At that point
  // `alpha` only reflects the moves that happened to be searched before the
  // clock ran out, so the node must NOT be written to the TT — a partial bound
  // stored at full depth outlives the iteration and poisons later searches.
  // Carried as state rather than re-testing shouldStop() at the store site so
  // the abort costs no extra clock read on the hot path.
  bool aborted = false;

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
