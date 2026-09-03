#ifndef PERPETUAL_H
#define PERPETUAL_H

#include <array>
#include <cstdint>
#include <vector>

#include "bitboard.h"

// Hard ceiling on prover ply, sized to makeMove's 256-entry undoInfo stack
// (bitboard.h). provesPerpetual() clamps any plyCap it is handed to this.
constexpr int PERPETUAL_PLY_LIMIT = 256;

// PerpetualStats::mateDist when the proof is an ordinary draw (repetition,
// stalemate, 50-move) rather than a forced mate. Any value >= 0 is a proven
// mate in that many plies from the prover's root.
constexpr int PERPETUAL_NO_MATE = -1;

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

  // Plies to a forced mate at the prover's root, or PERPETUAL_NO_MATE.
  //
  // The TRUE terminal set already includes "the defender is checkmated": the
  // attacker did better than a draw, which still satisfies a claim that was
  // only ever a LOWER bound of VALUE_DRAW. This field is what lets a caller
  // use the sharper fact instead of flattening it to VALUE_DRAW.
  //
  // Both directions of imprecision run the safe way, by construction:
  //   - it UNDER-reports. An OR node stops at the first check that holds, so a
  //     mate is seen only where the decisive check happened to be the mating
  //     one -- and that must hold at EVERY OR node of the proof tree, since an
  //     AND node needs all of its children to mate. What this reports is a
  //     floor on the real rate, never a ceiling.
  //   - the distance is an UPPER bound, for the same reason: it measures the
  //     line that was found, not the shortest one. So VALUE_MATE - mateDist
  //     remains a valid lower bound on the score.
  //
  // A mate verdict cannot rest on a repetition terminal -- an AND node ANDs
  // over all of its children, so a single repeating reply erases it -- so
  // unlike every draw proof in this system it carries no graph-history
  // dependency, and is storable without the GHI gate.
  int mateDist = PERPETUAL_NO_MATE;

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

// Cap on distinct cached positions held by one run. Once reached the table
// stops taking new keys but keeps upgrading the ones it has.
constexpr uint64_t PERPETUAL_MAX_CACHE = 4000000;

/**
 * Rate limiter on the in-search probe: prover nodes may not exceed
 * PERPETUAL_FREE_NODES + searchedNodes / PERPETUAL_NODE_SHARE_DIV.
 *
 * A per-PROBE budget is the wrong knob: lowering PERPETUAL_SEARCH_NODES just
 * trades cost-per-probe for probe count, because the faster search reaches more
 * gate-passing nodes. This is a different quantity -- a ceiling on the feature's
 * share of the WHOLE search.
 *
 * It exists because the prover's cost across positions is a thin tail, not a
 * level charge. The large majority of positions probe near-free; a small
 * minority, rich in checks, spend several times the search itself. The limiter
 * leaves the first group untouched and clips the second.
 *
 * Self-regulating rather than a hard stop: probing switches off while the
 * prover is ahead of its allowance and back on as the search catches up, so a
 * long search is never permanently locked out by one expensive early probe.
 *
 * The free floor is one full probe budget, so the first probe of a search
 * always runs (searchedNodes is ~0 at the root probe, and a ratio test alone
 * would make the limiter meaningless there).
 *
 * The divisor is deliberately loose. Throttling is not free: where the prover
 * is proving, its cutoffs prune whole subtrees and pay for themselves, so a
 * tight cap buys nodes/sec and loses depth. The share is set high enough that
 * the positions whose prover cost is already negligible search bit-identically,
 * and only the tail this was built for is clipped.
 */
constexpr uint64_t PERPETUAL_NODE_SHARE_DIV = 5;                       // <= 20%
constexpr uint64_t PERPETUAL_FREE_NODES     = PERPETUAL_SEARCH_NODES;


/**
 * The proof cache for one provesPerpetual() run.
 *
 * This was an std::unordered_map<Key, CacheEntry>, and the map -- not the proof
 * search -- was most of the prover's cost: with the cache disabled on the
 * same position it charged ~170 ns per node at the in-search budget and ~580
 * ns at the larger standalone ones, i.e. 37% and 62% of total time. Three
 * reasons, all structural rather than fixable by tuning: a malloc per entry,
 * an integer division per lookup (libstdc++ takes std::hash -- identity, for
 * uint64_t -- modulo a prime bucket count), and a dependent pointer load into a
 * table far too large to sit in cache.
 *
 * None of that is needed here. The key is a Zobrist hash, so the low bits are
 * already uniformly random and a power-of-two mask is as good as any prime; the
 * payload is 6 bytes, so a slot fits in 16 and four sit on a cache line; and
 * entries are never erased, so open addressing needs no tombstones.
 *
 * `epoch` is the part that makes it work for search rather than just for a
 * one-off run. A run must start from an empty table, and search issues
 * thousands of short probes -- memset-ing a megabyte for each would cost more
 * than the map it replaces. So a slot carries the id of the run that wrote it
 * and any slot not stamped with the current run reads as empty: newRun() is an
 * increment, and a real clear happens once per 65535 runs when the counter
 * wraps.
 *
 * The table is process-wide and grow-only rather than owned by ProveContext,
 * which is what drops the per-probe allocation to zero. Safe because the prover
 * is single-threaded and never re-entrant -- no caller runs inside an OpenMP
 * region, and a probe never starts another probe.
 *
 * Deliberately NOT a direct-mapped replace-on-collision table like
 * PerpetualFailCache below. That one can evict freely because a lost entry only
 * costs a skipped probe; here a lost entry changes node counts, and therefore
 * which proofs land inside the budget. Probing to a true empty slot keeps this
 * a drop-in whose per-cap node counts match the map exactly. Eviction would
 * not, and is a separate question.
 */
class PerpetualProofCache
{
  public:

  /**
   * One resolved position.
   *
   * TRUE entries are unconditional. A forced repetition is proven or it is not,
   * and how much room was left when it was found does not enter into it.
   *
   * FALSE entries carry the room they failed with: `remaining` is plyCap - ply
   * at the node that established the failure. A query with no more room than
   * that cannot do better, so the entry is reusable only when the caller's
   * remaining is <= this. Same idea as a transposition table's depth field,
   * counted from the other end -- and it is what lets one iterative-deepening
   * pass reuse the shallower passes instead of only itself.
   *
   * `mateDist` rides along on TRUE entries (PERPETUAL_NO_MATE on the rest). A
   * mate distance is absolute -- plies below this node -- so it is as
   * cap-independent as the TRUE it belongs to. Note that a TRUE stored WITHOUT
   * a mate permanently suppresses mate detection at every later transposition
   * into it: a cache hit returns before the node is expanded, and there is no
   * upgrade path back. That is the undercount compounding, and it is deliberate
   * -- every route out of it costs nodes, which this design spends none of.
   */
  struct Entry
  {
    Key      key       = 0;
    uint16_t epoch     = 0;      // run that wrote this slot; != current == empty
    int16_t  remaining = 0;
    int16_t  mateDist  = 0;
    bool     result    = false;
  };

  // Four slots to a 64-byte line, so a linear probe usually stays on the line
  // the first one landed on. The claim is worth pinning: adding a field or
  // widening one silently halves that.
  static_assert(sizeof(Entry) == 16, "PerpetualProofCache::Entry must stay 16 B");

  private:

  static constexpr size_t MIN_SLOTS = size_t(1) << 12;   //   4k x 16 B = 64 KB
  static constexpr size_t MAX_SLOTS = size_t(1) << 24;   //  16M x 16 B = 256 MB

  std::vector<Entry> table;
  size_t   mask  = 0;
  uint16_t epoch = 0;
  uint64_t live  = 0;   // distinct keys written during the current run
  uint64_t cap   = 0;   // most this run will hold; see newRun()

  public:

  /**
   * Begin a run expecting up to `wantEntries` distinct positions.
   *
   * Sized to twice that, so linear probing runs at a load factor of 0.5 or
   * below -- above ~0.7 the probe chains grow fast enough to give back what
   * open addressing won. Grow-only: search asks for the same 20000 every probe
   * and allocates once for the life of the process.
   *
   * `cap` is clamped to three quarters of the table on top of the caller's own
   * limit, which both guarantees a stale slot always exists (so a lookup for an
   * absent key terminates) and bounds the memory a large request can ask for. It cannot bind in practice: entries are bounded by nodes, nodes by
   * the node budget, and the table is sized from that same budget.
   */
  void
  newRun(uint64_t wantEntries) noexcept;

  // The entry for `key`, or nullptr. The caller applies the reuse rule -- an
  // entry being present does not mean it answers the question being asked.
  const Entry*
  probe(Key key) const noexcept
  {
    size_t i = size_t(key) & mask;

    while (table[i].epoch == epoch)
    {
      if (table[i].key == key)
        return &table[i];
      i = (i + 1) & mask;
    }
    return nullptr;
  }

  /**
   * Insert, or upgrade in place. Returns whether anything was written.
   *
   * A proof beats a failure, and a failure established with more room to work
   * in beats one established with less; a TRUE is never overwritten, being
   * unconditional already.
   */
  bool
  store(Key key, bool result, int16_t remaining, int16_t mateDist) noexcept
  {
    size_t i = size_t(key) & mask;

    while (table[i].epoch == epoch)
    {
      if (table[i].key == key)
      {
        Entry& e = table[i];

        if (e.result or (!result and remaining <= e.remaining))
          return false;

        e.result    = result;
        e.remaining = remaining;
        e.mateDist  = mateDist;
        return true;
      }
      i = (i + 1) & mask;
    }

    if (live >= cap)
      return false;             // full: stop growing, keep what we have

    table[i] = Entry{key, epoch, remaining, mateDist, result};
    ++live;
    return true;
  }

  uint64_t
  size() const noexcept
  { return live; }
};

extern PerpetualProofCache perpetualProofCache;

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
