

#ifndef ENDGAME_SOLVER_H
#define ENDGAME_SOLVER_H

#include <array>
#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include "bitboard.h"

/**
 * @brief Self-contained perfect WDL oracle for small material signatures.
 *
 * This is the "oracle" stage of the endgame-verdict validation harness. Given a
 * material signature it solves, by backward induction, the perfect win/draw/loss
 * value of EVERY legal position, then answers O(1) probes. Positions are decided
 * only by a forward check of their moves; after one full sweep, a backward move
 * generator picks which positions to re-check (the predecessors of those just
 * decided), so draws are not re-checked on every sweep.
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
 * Each table stores one entry per position up to board symmetry (see Layout),
 * about 1/8 of 64^n * 2 without pawns and under 1/2 with them. build() still
 * accepts at most 4 men.
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
  static constexpr int MAX_MEN = 5;

  // Canonical material signature: the men (incl. both kings) as sorted Pieces.
  // Slot i of a table is the man sig[i]; identical men are adjacent.
  using Sig = std::vector<Piece>;

  // Where each position of one table is stored. The board is first turned by
  // one of its symmetries so the white king lands in a fixed region: the
  // triangle a1-d1-d4 without pawns (any of the 8 symmetries), files a-d with
  // pawns (only the left-right mirror keeps pawn moves legal). The entry is then
  // the king pair's number, then each run of identical men as one number (a
  // pawn has 48 squares, ranks 2-7), then the side to move. See indexOf.
  struct Layout
  {
    int      n      = 0;       // men, kings included
    bool     pawns  = false;
    int      wk = 0, bk = 0;   // slots of the white and black king
    int      groups = 0;       // runs of identical men other than the kings
    std::array<int, MAX_MEN>      first{}, count{};  // slots of each run
    std::array<int, MAX_MEN>      offset{};          // 8 for pawns (squares a2..h7 -> 0..47), else 0
    std::array<uint64_t, MAX_MEN> size{};            // numbers each run can take
    uint64_t total  = 0;       // entries in the table
  };

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

  // WDL counts over the entries of one solved signature, so one per position up
  // to symmetry. Returns false if the signature was not solved.
  bool
  distribution(const std::vector<Piece>& extras,
               uint64_t& win, uint64_t& draw, uint64_t& loss) const;

  // ---- disk persistence ---------------------------------------------------
  // Each solved signature table is a pure function of the engine's move
  // generation, so it is cached to disk and reloaded verbatim on a later run.
  // Tables are keyed by their raw Piece bytes (case-proof on NTFS, unlike FEN
  // chars) under `cacheDir`. A loaded table is bit-identical to a freshly solved
  // one, so probe() is unaffected; the cache is an optimization that can never
  // feed wrong data (every header field + file size is re-validated on load).
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

  // Solve with the original method, which re-checks every undecided position on
  // every sweep. The default re-checks only the predecessors of positions decided
  // in the previous sweep; both reach the same tables. Kept as the reference for
  // timing and for checking the default.
  bool fullSweeps = false;

  // Always solve the target table, but read its sub-tables from the disk cache
  // (solving any that are missing), and write nothing. For timing and checking
  // one table at a time.
  bool solveTargetOnly = false;

  // One table of the last build(), in solve order.
  struct TableStats
  {
    Sig      sig;
    bool     loaded      = false;   // read from the disk cache, not solved
    uint64_t entries     = 0;       // size of the table
    double   seconds     = 0;       // solve time (0 when loaded)
    int      sweeps      = 0;
    uint64_t evaluations = 0;       // forward checks after the classification pass
  };
  std::vector<TableStats> lastBuild;

  // Compare a solved table with its cache file, byte for byte. Returns false if
  // the table is not solved or the file is missing or unreadable; otherwise sets
  // `differing` to the number of entries that differ.
  bool
  compareWithCache(const Sig& sig, uint64_t& differing) const;

  // Compare a solved table with a cache file in the format used before
  // 2026-10-05 (no symmetry, 64^n * 2 entries), position by position: every
  // legal position there must have the same value here, and every entry here
  // must be a legal position there. Returns false if the table is not solved or
  // there is no such file; otherwise `differing` counts the mismatches.
  bool
  compareWithOldCache(const Sig& sig, uint64_t& differing) const;

  // Test the backward move generator against the engine's own moves: for every
  // legal position P of `sig` and every move of P that stays in `sig` (neither a
  // capture nor a promotion), P must be among the predecessors of the position
  // the move reaches. Needs no solved tables. `edges` counts the moves checked,
  // `missing` the ones the generator missed; `example` describes the first miss.
  void
  checkPredecessors(const Sig& sig, uint64_t& edges, uint64_t& missing,
                    std::string& example) const;

private:
  struct Table
  {
    Layout           layout;
    std::vector<Wdl> wdl;
  };
  std::map<Sig, Table> registry;  // solved tables by signature

  // `cacheDir` anchored to the executable's directory when it is relative
  // (absolute paths pass through unchanged). Falls back to `cacheDir` verbatim
  // if the executable path can't be determined.
  std::string resolvedCacheDir() const;
  // Cache file path for a signature, or "" if there is no cache dir. (Callers
  // check `cacheEnabled` themselves; compareWithCache reads it regardless.)
  std::string cachePath(const Sig& sig) const;
  // Path of the signature's cache file in the format before 2026-10-05.
  std::string oldCachePath(const Sig& sig) const;
  // Read and validate the cache file of `sig` into `table`; false on any
  // miss/mismatch/IO error.
  bool readCacheFile(const Sig& sig, std::vector<Wdl>& table) const;
  // Try to load `sig` from disk straight into the registry; false on any
  // miss/mismatch/IO error (caller then solves and saves).
  bool cacheLoad(const Sig& sig);
  // Persist the already-solved registry[sig] (atomic temp-then-rename). Best
  // effort: IO failures are swallowed -- the cache is never load-bearing.
  void cacheSave(const Sig& sig);

  // Context for the table currently being solved (so same-signature successors
  // can read the partially-filled table during relaxation).
  Sig                currentSig;
  Layout             currentLayout;
  std::vector<Wdl>*  currentTable = nullptr;

  void solve(const Sig& sig, TableStats& stats);
  void relaxFullSweeps(const Sig& sig, const Layout& layout, std::vector<Wdl>& table,
                       TableStats& stats);
  void relaxFrontier(const Sig& sig, const Layout& layout, std::vector<Wdl>& table,
                     TableStats& stats);
  Wdl  valueOf(const ChessBoard& pos) const;
  Wdl  forwardValue(ChessBoard& pos) const;
};

#endif
