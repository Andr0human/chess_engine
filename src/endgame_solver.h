

#ifndef ENDGAME_SOLVER_H
#define ENDGAME_SOLVER_H

#include <array>
#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include "bitboard.h"

/**
 * @brief Exact win/draw/loss oracle for small material signatures.
 *
 * egvalidate uses this as its oracle. For a material signature it works out
 * the exact win/draw/loss value of every legal position by backward induction,
 * then answers each probe with one table lookup. A position is only decided by
 * checking its moves. After the first full sweep, a backward move generator
 * picks which positions to check again (the predecessors of the ones just
 * decided), so draws aren't checked again on every sweep.
 *
 * Values are from the side to move's point of view and assume unlimited play.
 * There is no 50-move rule, only whether the side to move can force mate, will
 * be mated, or neither. That matches isTheoreticalDraw, which has no move
 * counter.
 *
 * Captures and promotions change the signature, so a target like KPKB is
 * solved on top of smaller tables (KPK, KQKB, ... down to positions with
 * insufficient material). build() finds those tables, orders them by
 * (piece count, pawn count) and solves the smallest first. Every table is
 * cached.
 *
 * A table holds positions without an en passant square. A position with one
 * (after a double push next to an enemy pawn) also has the en passant capture,
 * which leads into a smaller table; enPassantValue combines the two.
 *
 * Each table stores one entry per position up to board symmetry (see Layout),
 * about 1/8 of 64^n * 2 without pawns and under 1/2 with them, at 2 bits per
 * entry once solved (a byte while solving, which also needs UNKNOWN).
 * Signatures of up to MAX_MEN men are supported.
 */

// Perfect verdict for one position, side-to-move relative.
enum class Wdl : uint8_t
{
  ILLEGAL = 0,   // not a real position (overlap, kings adjacent, side-not-to-move in check, ...)
  UNKNOWN = 1,   // only used while solving (probe never returns it)
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

  // Solve the target signature (the two kings plus `extras`) and every smaller
  // table its captures and promotions lead to, and cache them all. Returns
  // false and sets `err` if the signature has more than MAX_MEN men. A target
  // with insufficient material always succeeds (every probe is DRAW).
  bool
  build(const std::vector<Piece>& extras, std::string& err);

  // Exact WDL for a legal position whose signature has been solved (the target
  // or any of its smaller tables), en passant square included. Castling rights
  // are ignored. Never returns ILLEGAL or UNKNOWN for a legal position.
  Wdl
  probe(const ChessBoard& pos) const;

  // WDL counts over the entries of one solved signature, so one per position up
  // to symmetry. Returns false if the signature was not solved.
  bool
  distribution(const std::vector<Piece>& extras,
               uint64_t& win, uint64_t& draw, uint64_t& loss) const;

  // ---- disk cache ---------------------------------------------------------
  // A solved table depends only on the engine's move generation, so it is
  // saved to disk and loaded as is on later runs. Files under `cacheDir` are
  // named by the raw Piece bytes, because FEN letters would clash on NTFS,
  // which ignores case. A loaded table is identical to a freshly solved one.
  // Every header field and the file size are checked on load, so a bad file
  // is never used.
  //
  // A relative `cacheDir` is taken from the executable's folder, not the
  // working directory, so the cache is always next to the binary (the default
  // "egcache" is output/egcache for the normal build). Use an absolute path to
  // put it elsewhere.
  bool        cacheEnabled = true;
  std::string cacheDir     = "egcache";

  // How the last build() got its tables: solved, or loaded from disk. Lets the
  // caller report whether the cache was used without timing each table.
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

  // Test the backward move generator against the engine's own moves: for every
  // legal position P of `sig` and every move of P that stays in `sig` (neither a
  // capture nor a promotion), P must be among the predecessors of the position
  // the move reaches. Needs no solved tables. `edges` counts the moves checked,
  // `missing` the ones the generator missed; `example` describes the first miss.
  void
  checkPredecessors(const Sig& sig, uint64_t& edges, uint64_t& missing,
                    std::string& example) const;

  // Check a solved table against its own moves. An entry must be ILLEGAL
  // exactly when it isn't a legal position stored there. Otherwise it must
  // match one forward step over the solved tables: a win has a move to a loss,
  // a loss has only moves to wins (or is mate), and a draw has neither. The
  // solver only decides a position from positions already decided, so its wins
  // and losses are always right. This catches a draw that should have been
  // decided, which is what a missed predecessor would leave, so a table that
  // passes is correct. A move to a position with an en passant square is
  // valued by that position's own moves, not by enPassantValue.
  // Returns false if the table is not solved; otherwise `positions` counts the
  // legal positions, `wrong` the entries that fail, and `example` describes the
  // first failure.
  bool
  verify(const Sig& sig, uint64_t& positions, uint64_t& wrong,
         std::string& example) const;

private:
  // A solved table: entry i is bits 2(i%4)..2(i%4)+1 of byte i/4, coded 0..3
  // for ILLEGAL, LOSS, DRAW, WIN.
  struct Table
  {
    Layout               layout;
    std::vector<uint8_t> packed;

    Wdl
    at(uint64_t i) const
    {
      const unsigned c = (packed[i >> 2] >> ((i & 3) * 2)) & 3u;
      return c ? Wdl(c + 1) : Wdl::ILLEGAL;
    }
  };
  std::map<Sig, Table> registry;  // solved tables by signature

  // The last build()'s target and its table (null when the target needs none),
  // which probe() checks before searching the registry. Map entries never move.
  Sig          targetSig;
  const Table* targetTable = nullptr;

  // `cacheDir`, taken from the executable's folder if it is relative. An
  // absolute path is returned as is, and so is `cacheDir` if the executable's
  // path can't be found.
  std::string resolvedCacheDir() const;
  // Cache file path for a signature, or "" if there is no cache folder.
  // Callers check `cacheEnabled` themselves (compareWithCache reads the file
  // either way).
  std::string cachePath(const Sig& sig) const;
  // Read and check the cache file of `sig` into `packed`. False if the file is
  // missing, doesn't match, or can't be read.
  bool readCacheFile(const Sig& sig, std::vector<uint8_t>& packed) const;
  // Load `sig` from disk into the registry. False if the file is missing,
  // doesn't match, or can't be read; the caller then solves and saves it.
  bool cacheLoad(const Sig& sig);
  // Save the solved registry[sig], writing a temp file and renaming it. Write
  // errors are ignored, since the cache only saves time.
  void cacheSave(const Sig& sig);

  // The table being solved, so moves that stay in the same signature can read
  // the partly filled table.
  Sig                currentSig;
  Layout             currentLayout;
  std::vector<Wdl>*  currentTable = nullptr;

  void solve(const Sig& sig, TableStats& stats);
  void relaxFullSweeps(const Sig& sig, const Layout& layout, std::vector<Wdl>& table,
                       TableStats& stats);
  void relaxFrontier(const Sig& sig, const Layout& layout, std::vector<Wdl>& table,
                     TableStats& stats);
  Wdl  valueOf(const ChessBoard& pos) const;
  template <typename Own>
  Wdl  valueIn(const ChessBoard& pos, const Sig& sig, const Layout& layout,
               const Own& own) const;
  Wdl  enPassantValue(const ChessBoard& pos, Wdl v) const;
  Wdl  forwardValue(ChessBoard& pos) const;
};

#endif
