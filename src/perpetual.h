#ifndef PERPETUAL_H
#define PERPETUAL_H

#include <array>
#include <cstdint>

#include "bitboard.h"

// Hard ceiling on prover ply, sized to the 256-entry undoInfo stack that
// makeMove pushes onto (bitboard.h). The prover's root already sits some plies
// into the real game history, so the usable depth is lower -- provesPerpetual()
// clamps whatever plyCap it is handed to this.
constexpr int PERPETUAL_PLY_LIMIT = 256;

/**
 * Statistics from one provesPerpetual() run.
 *
 * `maxPly` is the deepest ply the DFS reached, measured from the prover's root.
 * It is meaningful mainly when `budgetHit` is true -- see the caveat in
 * provesPerpetual()'s comment about reading it as a drawishness signal.
 *
 * `nodesAtPly` is the per-ply node histogram: nodesAtPly[k] counts nodes
 * expanded at ply k. Successive ratios give the proof tree's branching factor
 * directly, without needing a second run at a deeper cap. Expect the ratio to
 * oscillate -- OR plies fan out over the attacker's checks, AND plies over the
 * defender's evasions, and those two counts are not alike.
 *
 * The cache counters are the diagnostic for whether memoisation is doing any
 * work. `cacheStores` exceeds `cacheEntries` because an entry can be upgraded
 * in place. `truePathBound` is the one to watch when the cache underperforms:
 * it counts proofs that were correct but unstorable, because they leaned on a
 * repetition owned by an ancestor above the node -- the graph-history-
 * interaction tax, paid in hit rate rather than in soundness.
 */
struct PerpetualStats
{
  uint64_t nodes     = 0;      // nodes consumed (charged against the budget)
  int      maxPly    = 0;      // deepest ply reached, from the prover root
  bool     budgetHit = false;  // did any branch bail on the node/ply cap?

  // The attacker's proving move at the prover's root, or NULL_MOVE. Only set
  // when the run returned TRUE and the proof rests on an actual move -- a root
  // stalemate is a draw with no move to name. Filled at ply 0 only; the deeper
  // OR nodes' choices are not recorded, so this names the first move of a
  // proof, not the whole line.
  Move proofMove = NULL_MOVE;

  uint64_t cacheHits     = 0;  // node visits answered without expanding
  uint64_t cacheStores   = 0;  // entries written or upgraded
  uint64_t cacheEntries  = 0;  // distinct keys held at the end of the run
  uint64_t truePathBound = 0;  // proofs withheld from the cache by the GHI rule

  std::array<uint64_t, PERPETUAL_PLY_LIMIT> nodesAtPly{};
};

// Default budgets for a standalone probe.
constexpr uint64_t PERPETUAL_MAX_NODES = 20000;
constexpr int      PERPETUAL_MAX_PLY   = 100;

/**
 * How the attacker's checks are ordered at an OR node.
 *
 * Pure search economy -- an OR node stops at the first check that holds, so the
 * order decides how many siblings are expanded before one sticks. It can never
 * change the verdict, which makes it the one knob that is safe to tune on node
 * counts alone.
 *
 * NEAR/FAR are manhattan distance from the moving piece's destination to the
 * enemy king. Which one wins is a property of the checking PIECE, not of the
 * heuristic: a knight or a pawn has to be adjacent to check at all, while a
 * rook or queen perpetual works by checking from a distance the king cannot
 * close and no piece can capture into. Measure, do not assume.
 *
 * Distance is the WHOLE key -- it does not know whether the check is a capture,
 * whether the checking piece lands defended, or whether the defender can simply
 * take it. A NEAR sort will happily rank a rook check delivered next to the
 * enemy king above one from across the board even when the near square is
 * covered and the far one is not. That is not an oversight to route around with
 * distance tweaks: the honest fix is a safety key (SEE, or "is to_sq attacked
 * by the defender"), which is a separate experiment.
 *
 * Splitting the near band on whether the check is a CAPTURE was tried and
 * dropped: near-quiet > near-capture > far and near-quiet > far > near-capture
 * both landed within a few percent of plain NEAR on the Rxf2 fortress, with the
 * sign flipping between ply caps. The near/far half of that split could not
 * contribute at all there -- the attacker is a lone rook, so every check lands
 * on the king's rank or file, where chebyshev and manhattan distance coincide
 * and any "near band" is just a prefix of the plain NEAR order. Worth retrying
 * only on a fortress whose attacker checks off-line (knight, queen diagonal).
 *
*/
enum class CheckOrder { NONE, NEAR, FAR };

/**
 * How the defender's evasions are ordered at an AND node.
 *
 * The mirror of CheckOrder, and it pays under the mirror condition: an AND node
 * stops at the first evasion that BREAKS OUT, so ordering only earns anything at
 * nodes that turn out to be refutable. A node where every evasion stays trapped
 * expands all of them whatever the order -- and those are the nodes the proof is
 * actually made of, so do not expect this to move the ply wall.
 *
 * Both keys are measured against the square the checking piece just moved to,
 * which the parent hands down. Two cases where that square is a lie: a
 * DISCOVERED check, where the checker is some other piece entirely, and an
 * en-passant capture of a checking pawn, whose destination is not the pawn's
 * square. Both are rare enough to leave mis-sorted rather than pay a probe.
 *
 *   CAPTURE  take the checker first -- the most decisive way out of a check net
 *   FLEE     farthest from the checker first -- run, do not shuffle
 *   BOTH     CAPTURE, then FLEE among the rest
 *   APPROACH nearest the checker first -- FLEE's mirror, and the one that works
 *
 * Measured on the Rxf2 fortress, FLEE costs 8x the nodes of movegen order and
 * CAPTURE is bit-identical to it (in a rook perpetual the checker is never
 * capturable, so the key is constant). That FLEE is that much WORSE is the
 * useful part: distance-to-checker is a real signal read backwards. Walking
 * away from a rook does not escape it, so FLEE front-loads exactly the moves
 * that stay trapped; APPROACH is the same key with the comparator flipped.
 */
enum class EvasionOrder { NONE, CAPTURE, FLEE, BOTH, APPROACH };

/**
 * Budgets for the in-search probe (alphaBeta), which is a different question
 * from the standalone one: there the ask is "is this position a draw, given
 * time to find out", here it is "can I refute this line cheaply".
 *
 * The ply cap does the work, not the node budget -- a loose cap lets the
 * failing branches run to full depth before the winning one closes, so cap 30
 * proves the reference node below in 41k nodes where cap 50 needs 8.3M.
 *
 * The cap is also what keeps the probe inside ChessBoard's 256-entry undoInfo
 * stack. Game history is bounded by the halfmove clock (undoInfoPush resets on
 * every capture and pawn move when !inSearch), so the worst case is 100 game
 * plies + MAX_PLY search plies + this cap, which must stay under 256.
 *
 * Cap 25 and CAPTURE ordering are measured on the Qxe8 reference node, and the
 * cap is a knife edge there rather than a plateau: caps 25 and 26 prove it in
 * 9.0k and 12.8k nodes, cap 24 and caps 28+ do not prove it inside 20k at all.
 * The standalone defaults (cap 100, APPROACH) prove NOTHING at this budget --
 * APPROACH fails at every cap from 20 to 30. That flip is not a contradiction
 * of the standalone measurements, it is the same finding read at a budget 3
 * orders of magnitude smaller: APPROACH was tuned on the Rxf2 fortress, where
 * the checking rook is never capturable and the key is therefore constant.
 * Here the attacker's queen checks ARE capturable, so "take the checker" is
 * live information and "walk toward it" is noise.
 */
constexpr uint64_t     PERPETUAL_SEARCH_NODES   = 20000;
constexpr int          PERPETUAL_SEARCH_PLY_CAP = 25;
constexpr CheckOrder   PERPETUAL_SEARCH_ORDER   = CheckOrder::NEAR;
constexpr EvasionOrder PERPETUAL_SEARCH_EVASION = EvasionOrder::CAPTURE;

// Cap on distinct cached positions. The table is an unordered_map, so budget
// roughly 48 bytes per entry -- 4M entries is a bit under 200 MB. Once full it
// stops taking new keys but keeps upgrading the ones it has.
constexpr uint64_t PERPETUAL_MAX_CACHE = 4000000;

/**
 * @brief Can the side to move force an unending check sequence?
 *
 * A boolean AND/OR proof search restricted to checking moves for the side to
 * move at entry (the "attacker"). The attacker needs ONE check that holds; the
 * defender -- always in check inside this search, so never granted a free move
 * -- needs ONE escape that breaks out.
 *
 * Terminals, all of which are real draws under the rules:
 *   - repetition (2-fold is proof enough: the attacker only wants a draw and
 *     can simply repeat)
 *   - stalemate, of either side -- in practice the attacker's, after the
 *     defender captures the checking piece
 *   - the 50-move clock expires
 * A branch fails when the attacker has no check available.
 *
 * Fails closed: budget or ply-cap exhaustion returns false ("unknown"), never
 * a false draw claim. The cache preserves that direction -- see the comments in
 * perpetual.cpp -- so `useCache` can cost proofs but can never invent one.
 *
 * @param pos          board, mutated and restored (every makeMove is paired)
 * @param stats        out-param, see PerpetualStats
 * @param order        how to sort the attacker's checks; see CheckOrder.
 * @param evasion      how to sort the defender's replies; see EvasionOrder.
 */
bool
provesPerpetual(ChessBoard& pos, PerpetualStats& stats,
                uint64_t nodeBudget  = PERPETUAL_MAX_NODES,
                int      plyCap      = PERPETUAL_MAX_PLY,
                bool     useCache    = true,
                uint64_t cacheCap    = PERPETUAL_MAX_CACHE,
                CheckOrder   order   = CheckOrder::NEAR,
                EvasionOrder evasion = EvasionOrder::APPROACH);

#endif
