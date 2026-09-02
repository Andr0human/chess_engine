#ifndef PERPETUAL_H
#define PERPETUAL_H

#include <array>
#include <cstdint>

#include "bitboard.h"

// Hard ceiling on prover ply, sized to makeMove's 256-entry undoInfo stack
// (bitboard.h). provesPerpetual() clamps any plyCap it is handed to this.
constexpr int PERPETUAL_PLY_LIMIT = 256;

/**
 * Statistics from one provesPerpetual() run.
 *
 * `nodesAtPly[k]` counts nodes expanded at ply k; successive ratios give the
 * proof tree's branching factor. Expect it to oscillate -- OR plies fan out
 * over checks, AND plies over evasions.
 *
 * `truePathBound` is the one to watch when the cache underperforms: proofs
 * that were correct but unstorable because they leaned on a repetition owned
 * by an ancestor (graph-history interaction), paid in hit rate not soundness.
 */
struct PerpetualStats
{
  uint64_t nodes     = 0;      // nodes consumed (charged against the budget)
  int      maxPly    = 0;      // deepest ply reached, from the prover root
  bool     budgetHit = false;  // did any branch bail on the node/ply cap?

  // The attacker's proving move at the prover's root, or NULL_MOVE. Set only
  // on a TRUE run that rests on a move (a root stalemate names none), and only
  // at ply 0 -- the first move of a proof, not the whole line.
  Move proofMove = NULL_MOVE;

  uint64_t cacheHits     = 0;  // node visits answered without expanding
  uint64_t cacheStores   = 0;  // entries written or upgraded (> cacheEntries)
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
 * Pure search economy -- an OR node stops at the first check that holds, so
 * the order only decides how many siblings are expanded. It cannot change the
 * verdict, so it is safe to tune on node counts alone.
 *
 *   NEAR/FAR    manhattan distance from the mover's destination to the enemy
 *               king. Which one wins depends on the checking PIECE (a knight
 *               must be adjacent; a rook checks from afar), so measure.
 *   HEAVY       heaviest checking piece first -- perpetuals are usually driven
 *               by a queen or rook. Movegen order inside a value class.
 *   HEAVY_NEAR  HEAVY, ties broken by NEAR. Shipped default.
 *
 * The keys are complements, not substitutes: HEAVY alone is worse than NEAR on
 * both axes. Value picks the piece that can sustain a perpetual, distance
 * picks where to check with it.
 *
 * Distance is the whole key -- it ignores whether the check is a capture,
 * lands defended, or is simply takeable. The honest fix is a safety key (SEE,
 * or "is to_sq defended"), not more distance tweaks. Splitting the near band
 * on capture/quiet was tried both ways and landed on top of plain NEAR.
 *
 * Compare orderings at FIXED depth (time-based arms search different trees)
 * and judge on proofs AND nodes: an order that gives up earlier looks cheap
 * while proving less -- that is exactly how FAR reads.
 *
 * checkerValue() returns 0 for a KING move flagged as a check: that is by
 * definition a discovered check, so the mover is not the real checker.
 */
enum class CheckOrder { NONE, NEAR, FAR, HEAVY, HEAVY_NEAR };

/**
 * How the defender's evasions are ordered at an AND node.
 *
 * The mirror of CheckOrder: an AND node stops at the first evasion that breaks
 * out, so this only earns anything at refutable nodes. Nodes where every
 * evasion stays trapped -- the ones the proof is made of -- expand all of them
 * regardless, so do not expect this to move the ply wall.
 *
 *   CAPTURE  take the checker first -- the most decisive way out
 *   FLEE     farthest from the checker first
 *   BOTH     CAPTURE, then FLEE among the rest
 *   APPROACH nearest the checker first -- FLEE's mirror, and the one that works
 *
 * FLEE costing many times movegen order is the useful part: walking away from
 * a rook does not escape it, so FLEE front-loads exactly the moves that stay
 * trapped. CAPTURE collapses onto movegen order wherever the checker is never
 * capturable (the key is constant there).
 *
 * Both keys measure against the square the checking piece moved to, which the
 * parent hands down. That square is a lie for a discovered check and for an
 * en-passant capture of a checking pawn -- both rare enough to leave
 * mis-sorted rather than pay a probe.
 */
enum class EvasionOrder { NONE, CAPTURE, FLEE, BOTH, APPROACH };

/**
 * Budgets for the in-search probe (alphaBeta), a different question from the
 * standalone one: not "is this a draw, given time" but "can I refute this line
 * cheaply".
 *
 * The ply cap does the work, not the node budget -- a loose cap lets failing
 * branches run to full depth before the winning one closes, so a tighter cap
 * can cheaply prove a node a looser one gives up on. It is a knife edge, not a
 * plateau: a ply either side can lose a proof.
 *
 * The cap also keeps the probe inside the 256-entry undoInfo stack: game
 * history is bounded by the halfmove clock, so the worst case is 100 game
 * plies + MAX_PLY + this cap, which must stay under 256.
 *
 * The standalone defaults (looser cap, APPROACH) prove nothing here. Not a
 * contradiction: APPROACH was tuned where the checking piece is never
 * capturable, so the CAPTURE key was constant. Here the attacker's checks ARE
 * capturable, which makes "take the checker" live information.
 */
constexpr uint64_t     PERPETUAL_SEARCH_NODES   = 20000;
constexpr int          PERPETUAL_SEARCH_PLY_CAP = 25;
constexpr CheckOrder   PERPETUAL_SEARCH_ORDER   = CheckOrder::HEAVY_NEAR;
constexpr EvasionOrder PERPETUAL_SEARCH_EVASION = EvasionOrder::CAPTURE;

// Cap on distinct cached positions. The table is an unordered_map, so budget
// roughly 48 bytes per entry -- 4M entries is a bit under 200 MB. Once full it
// stops taking new keys but keeps upgrading the ones it has.
constexpr uint64_t PERPETUAL_MAX_CACHE = 4000000;

/**
 * Probe-suppression table for the in-search gate (alphaBeta).
 *
 * Not a transposition table: an entry says only "the prover already failed on
 * this position at this budget", and its one effect is to skip a probe.
 * Nothing is ever returned from it, so it needs no soundness argument -- a
 * stale or colliding entry costs a proof, never an unearned draw score.
 *
 * That is also why only FALSE lives here. A proof is path-dependent (see the
 * GHI note on provesPerpetual's own cache), and extra path history can only
 * push a verdict toward true, since more ancestors means more repetition hits.
 * So a FALSE reused on a richer path is at worst pessimistic, while a reused
 * TRUE would be a draw claim nothing earned.
 *
 * It exists because failing probes dominate the prover's cost and mostly
 * re-ask the same node: a proof is deliberately not stored in the TT, so
 * iterative deepening re-probes every iteration, PVS every re-search, and
 * every transposition from its own path.
 *
 * Keyed on ChessBoard::hashValue, which excludes the halfmove clock even
 * though the prover's terminals include the 50-move rule -- so two positions
 * alike but for the clock share a slot. That asks a coarser question than the
 * one being cached, but it can only suppress a probe, so it stays on the safe
 * side of the asymmetry above.
 *
 * Direct-mapped, full key stored, unconditional replacement: a slot either
 * matches exactly or misses (no verification scheme to get wrong), and it
 * keeps the most recently failed node, which is the one about to be re-asked.
 *
 * Cleared per search, alongside clearKillers()/clearHistory(). Carrying it
 * between moves would mostly be valid -- a failure stays a failure while the
 * position stands -- but it would make probe counts depend on game history and
 * stop measurements reproducing.
 */
class PerpetualFailCache
{
  static constexpr size_t BITS = 15;                 // 32k slots ...
  static constexpr size_t SIZE = size_t(1) << BITS;  // ... x 8 B = 256 KB
  static constexpr size_t MASK = SIZE - 1;

  // Zero doubles as "empty", so a position hashing to exactly 0 reads as a
  // failure it never earned -- one key in 2^64, costing a missed proof, left
  // unhandled rather than paid for on every lookup.
  std::array<Key, SIZE> table{};

  public:

  void
  clear() noexcept;

  // Did the prover already fail on this position, earlier in this search?
  bool
  failed(Key key) const noexcept
  { return table[key & MASK] == key; }

  void
  recordFail(Key key) noexcept
  { table[key & MASK] = key; }
};

extern PerpetualFailCache perpetualFailCache;


/**
 * @brief Can the side to move force an unending check sequence?
 *
 * A boolean AND/OR proof search restricted to checking moves for the side to
 * move at entry (the "attacker"). The attacker needs ONE check that holds; the
 * defender -- always in check here, so never granted a free move -- needs ONE
 * escape that breaks out. A branch fails when the attacker has no check.
 *
 * Terminals, all real draws under the rules:
 *   - repetition (2-fold is proof enough: the attacker only wants a draw and
 *     can simply repeat)
 *   - stalemate of either side -- in practice the attacker's, after the
 *     defender captures the checking piece
 *   - the 50-move clock expires
 *
 * Fails closed: budget or ply-cap exhaustion returns false ("unknown"), never
 * a false draw claim. The cache preserves that direction (see perpetual.cpp),
 * so `useCache` can cost proofs but never invent one.
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
