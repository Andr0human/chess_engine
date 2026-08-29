
#ifndef NODE_STATE_H
#define NODE_STATE_H

#include "types.h"
#include "varray.h"
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

  // Quiet moves searched at this node that did NOT cause a cutoff -- the malus
  // list. Filled across *all* stages (quiet checks, killers, residual quiets)
  // because playAllMoves threads one NodeState& through the stage recursion;
  // a per-stage span would be free but a QUIET-stage cutoff would then never
  // penalize the killers that failed ahead of it, which are the node's
  // highest-information failures. Drained exactly once, by the move that cuts
  // off. Fixed capacity: Varray::add() bounds-checks itself, so overflow
  // silently stops recording -- it costs a penalty, never correctness.
  Varray<Move, 64> triedQuiets{};

  constexpr int pvNextIndex() const noexcept { return pvIndex + MAX_PLY - ply; }

  // The quiet-futility skip test, in one place because two sites must agree on
  // it: playSubsetMoves breaks out of the QUIET stage on it, and playAllMoves
  // reads it to decide whether ordering that stage is worth paying for. Let the
  // two expressions drift apart and the failure isn't a wasted sort — it's an
  // unsorted band that does get searched, i.e. a silent move-ordering change.
  constexpr bool skipsQuiets(Move bestMove) const noexcept
  { return quietFutile and bestMove != NULL_MOVE; }
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
