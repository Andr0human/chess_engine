

#ifndef ENDGAME_SOLVER_H
#define ENDGAME_SOLVER_H

#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include "bitboard.h"

/**
 * @brief Self-contained perfect WDL oracle for small material signatures.
 *
 * This is the "oracle" stage of the endgame-verdict validation harness. Given a
 * material signature it solves, by backward induction (implemented as forward
 * Bellman-Ford relaxation to a fixpoint), the perfect win/draw/loss value of
 * EVERY legal position, then answers O(1) probes.
 *
 * Verdicts are **side-to-move relative** and computed under **infinite play**:
 * there is no 50-move / DTZ notion, only "with unlimited time, can the side to
 * move force mate / be forced to lose / neither". That is the correct match to
 * isTheoreticalDraw, which is a static recognizer with no move counter.
 *
 * Captures and promotions leave the signature, so a target like KPKB is solved
 * on top of a small DAG of sub-tablebases (KPK, KQKB, ... down to the
 * insufficient-material leaves). build() discovers that DAG, orders it by
 * (piece count, pawn count), and solves bottom-up; everything is cached.
 *
 * Scope: up to 4 men (the unfolded state space is 64^n * 2). 5-man needs
 * symmetry reduction and is intentionally out of scope for v1.
 */

// Perfect verdict for one position, side-to-move relative.
enum class Wdl : uint8_t
{
  ILLEGAL = 0,   // not a real position (overlap, kings adjacent, side-not-to-move in check, ...)
  UNKNOWN = 1,   // transient: undecided during relaxation (never returned by probe)
  LOSS    = 2,   // side to move is lost (mated under best play)
  DRAW    = 3,
  WIN     = 4,   // side to move wins under best play
};

class EgSolver
{
public:
  // Canonical material signature: the men (incl. both kings) as sorted Pieces.
  using Sig = std::vector<Piece>;

  // Solve the whole capture/promotion DAG of the target signature (the two
  // kings plus `extras`) and cache every table. Returns false + sets `err` if
  // the signature is unsupported (more than 4 men). Insufficient-material
  // targets succeed trivially (every probe is DRAW).
  bool
  build(const std::vector<Piece>& extras, std::string& err);

  // Perfect WDL for an arbitrary legal position whose signature (or a position
  // reachable from the built target) has been solved. Never returns
  // ILLEGAL/UNKNOWN for a legal input.
  Wdl
  probe(const ChessBoard& pos) const;

  // WDL distribution over all decided (legal, non-terminal-or-terminal) entries
  // of one solved signature -- a sanity cross-check (e.g. KPK win/draw counts).
  // Returns false if the signature was not solved.
  bool
  distribution(const std::vector<Piece>& extras,
               uint64_t& win, uint64_t& draw, uint64_t& loss) const;

  // ---- disk persistence ---------------------------------------------------
  // Each solved signature table is a pure function of the engine's move
  // generation, so it is cached to disk and reloaded verbatim on a later run --
  // turning a 60-90 s rebuild into a sub-second load. Tables are keyed by their
  // raw Piece bytes (case-proof on NTFS, unlike FEN chars) under `cacheDir`.
  // A loaded table is bit-identical to a freshly solved one, so probe() is
  // unaffected; the cache is an optimization that can never feed wrong data
  // (every header field + file size is re-validated on load).
  //
  // A relative `cacheDir` is anchored to the *executable's* directory, not the
  // process CWD, so the cache always lands beside the binary (the default
  // "egcache" => <exe-dir>/egcache, i.e. output/egcache for the normal build)
  // regardless of where elsa is launched from. Set an absolute path to override.
  bool        cacheEnabled = true;
  std::string cacheDir     = "egcache";

  // How the last build() resolved its DAG: tables computed vs loaded from disk.
  // Lets the caller report warm-vs-cold without timing each table.
  int tablesSolved = 0;
  int tablesLoaded = 0;

private:
  std::map<Sig, std::vector<Wdl>> registry;  // solved tables by signature

  // `cacheDir` anchored to the executable's directory when it is relative
  // (absolute paths pass through unchanged). Falls back to `cacheDir` verbatim
  // if the executable path can't be determined.
  std::string resolvedCacheDir() const;
  // Cache file path for a signature, or "" if caching is disabled / no dir.
  std::string cachePath(const Sig& sig) const;
  // Try to load `sig` from disk straight into the registry; false on any
  // miss/mismatch/IO error (caller then solves and saves).
  bool cacheLoad(const Sig& sig);
  // Persist the already-solved registry[sig] (atomic temp-then-rename). Best
  // effort: IO failures are swallowed -- the cache is never load-bearing.
  void cacheSave(const Sig& sig);

  // Context for the table currently being solved (so same-signature successors
  // can read the partially-filled table during relaxation).
  Sig                currentSig;
  std::vector<Wdl>*  currentTable = nullptr;

  void solve(const Sig& sig);
  Wdl  valueOf(const ChessBoard& pos) const;
};

#endif
