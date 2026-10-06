
#ifndef NODE_STATE_H
#define NODE_STATE_H

#include "types.h"
#include <optional>


/**
 * What a parent passes to alphaBeta: the window, depth left, ply, PV row and
 * extension count. Small, so it is passed by value. child() builds a child's
 * context: it flips the window to (-beta, -alpha) and steps the ply and PV row.
 */
struct SearchContext
{
  Score alpha;
  Score beta;
  Depth depth;
  Ply ply;
  int pvIndex;
  int numExtensions;
  bool doNull = true;

  // Whether the node is a PV node isn't stored here. It's a template parameter
  // (`template <bool PvNode>` on alphaBeta and the play* helpers), because a
  // child always gets either its parent's PvNode or false.

  constexpr int pvNextIndex() const noexcept { return pvIndex + MAX_PLY - ply; }

  // Context for a child searched to childDepth. Pass the window [a, b] as this
  // node sees it, and child() flips it. numExtensions is copied (including this
  // node's extension, once it has been applied), and doNull resets to true.
  constexpr SearchContext
  child(Depth childDepth, Score a, Score b) const noexcept
  { return SearchContext{-b, -a, childDepth, ply + 1, pvNextIndex(), numExtensions, true}; }

  constexpr SearchContext
  withoutNull() const noexcept
  {
    SearchContext ctx = *this;
    ctx.doNull = false;
    return ctx;
  }
};


/**
 * Per-node state used by playAllMoves, playSubsetMoves and playMove. It adds
 * to the node's SearchContext. alpha and hashf change as moves raise alpha or
 * cut off. depth and numExtensions change once, when the extension is applied.
 * The rest stay fixed within a node.
 */
struct NodeState : SearchContext
{
  Flag hashf = Flag::HASH_ALPHA;

  // Static eval, computed the first time it's needed and reused by RFP,
  // razoring and futility. nullopt until then.
  std::optional<Score> staticEval = std::nullopt;

  // Set in alphaBeta when the static eval is a depth-scaled margin below alpha.
  // playSubsetMoves then skips the remaining quiet moves once one move has been
  // searched.
  bool quietFutile = false;

  // Set when playSubsetMoves stops early on shouldStop(). alpha then only
  // covers the moves searched before time ran out, so the node must not be
  // stored in the TT, where the partial result would outlive this iteration.
  // A flag means the store doesn't need another clock check.
  bool aborted = false;

  // Whether to skip the remaining quiet moves. playSubsetMoves stops the QUIET
  // stage on it, and playAllMoves uses it to skip sorting that stage. Both must
  // use the same test, or a stage could be searched without being sorted.
  constexpr bool skipsQuiets(Move bestMove) const noexcept
  { return quietFutile and bestMove != NULL_MOVE; }
};


/**
 * Result of searching the TT hash move first in alphaBeta.
 *   searched = true:  the move was legal and searched. The caller must remove
 *                     it from myMoves so it isn't searched again.
 *   result has value: alphaBeta should return it (a timeout, or a beta cutoff
 *                     that is already stored in the TT).
 */
struct HashMoveOutcome
{
  bool searched = false;
  std::optional<Score> result;
};


#endif
