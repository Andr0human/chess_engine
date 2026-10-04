
#include <algorithm>
#include <array>
#include <queue>
#include <set>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

// For locating the running executable, so the disk cache anchors beside the
// binary rather than to the (caller-controlled) working directory.
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX           // keep std::min/std::max usable (windows.h defines macros)
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__APPLE__)
#  include <climits>
#  include <mach-o/dyld.h>
#else
#  include <climits>
#  include <unistd.h>
#endif

#include "endgame_solver.h"
#include "attacks.h"
#include "movegen.h"
#include "move_utils.h"
#include "perf.h"

using Sig = EgSolver::Sig;

namespace {

// On-disk cache format. MAGIC tags the layout; SOLVER_VERSION tags the *meaning*
// of the bytes and MUST be bumped whenever the solver, move generation, or
// legality test changes, so stale tables from an older engine are rejected
// rather than silently trusted. Either mismatch => the file is ignored and the
// table is re-solved. (Escape hatch for an un-versioned change: `nocache`.)
constexpr char     CACHE_MAGIC[4]   = { 'E', 'G', 'W', '1' };
constexpr uint32_t SOLVER_VERSION   = 1;

// Fixed-size header prefixed to the raw Wdl bytes. Every field is re-validated
// on load (incl. the signature itself and the trailing byte count), so a
// collision or truncated/partial file can never feed the oracle wrong data.
struct CacheHeader
{
  char     magic[4];
  uint32_t solverVer;
  uint32_t n;          // men count
  uint32_t pieces[4];  // the canonical signature (NO_PIECE-padded)
  uint64_t total;      // == 64^n * 2 == number of Wdl bytes that follow
};

// Absolute directory of the running executable, or an empty path if it can't
// be determined. Lets the cache anchor beside the binary instead of the CWD,
// so launching elsa from any directory reuses the same cache files.
std::filesystem::path
exeDir()
{
#if defined(_WIN32)
  wchar_t buf[MAX_PATH];
  const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) return {};
  return std::filesystem::path(std::wstring(buf, len)).parent_path();
#elif defined(__APPLE__)
  char buf[PATH_MAX];
  uint32_t size = sizeof buf;
  if (_NSGetExecutablePath(buf, &size) != 0) return {};
  std::error_code ec;
  const std::filesystem::path canon = std::filesystem::canonical(buf, ec);
  return (ec ? std::filesystem::path(buf) : canon).parent_path();
#else
  char buf[PATH_MAX];
  const ssize_t len = readlink("/proc/self/exe", buf, sizeof buf);
  if (len <= 0 || len >= static_cast<ssize_t>(sizeof buf)) return {};
  return std::filesystem::path(std::string(buf, static_cast<size_t>(len))).parent_path();
#endif
}

// ---- small geometry / signature helpers ---------------------------------

int
kingDistance(int a, int b)
{
  int r1 = a >> 3, f1 = a & 7;
  int r2 = b >> 3, f2 = b & 7;
  return std::max(std::abs(r1 - r2), std::abs(f1 - f2));
}

// Sort a multiset of men into the canonical slot order (by raw Piece value).
Sig
canonical(Sig v)
{
  std::sort(v.begin(), v.end());
  return v;
}

int
pawnCount(const Sig& sig)
{
  int n = 0;
  for (Piece p : sig)
    if (type_of(p) == PAWN) ++n;
  return n;
}

// Material with which no checkmate position exists at all -> every position is
// a draw, no table needed. (Bare kings, or one lone minor.) NOTE: this is
// strictly the "no mate is even constructible" set: KK, KNK, KBK. Two minors,
// a rook, a queen, or a pawn all admit mate positions and must be solved.
// Core takes a raw men span so the hot path (valueOf) can avoid a Sig alloc.
bool
insufficient(const Piece* men, int n)
{
  int nonKing = 0;
  PieceType lone = NONE;
  for (int i = 0; i < n; ++i)
    if (type_of(men[i]) != KING) { ++nonKing; lone = type_of(men[i]); }

  if (nonKing == 0) return true;
  if (nonKing == 1 && (lone == BISHOP || lone == KNIGHT)) return true;
  return false;
}

bool
insufficient(const Sig& sig)
{ return insufficient(sig.data(), static_cast<int>(sig.size())); }

bool
stmInCheck(const ChessBoard& pos, Color stm)
{ return (stm == WHITE) ? inCheck<WHITE>(pos) : inCheck<BLACK>(pos); }

bool
sideNotToMoveInCheck(const ChessBoard& pos, Color stm)
{ return (stm == WHITE) ? inCheck<BLACK>(pos) : inCheck<WHITE>(pos); }

// Signatures reachable from `sig` by ONE capture or ONE promotion. The
// transitive closure of this relation reaches every signature reachable by any
// single legal move (a combined capture-promotion = promote then capture), so
// it is enough to discover the whole sub-tablebase DAG.
void
childSignatures(const Sig& sig, std::set<Sig>& out)
{
  const int n = static_cast<int>(sig.size());

  // Captures: remove one non-king man.
  for (int i = 0; i < n; ++i)
  {
    if (type_of(sig[i]) == KING) continue;
    Sig c;
    for (int j = 0; j < n; ++j)
      if (j != i) c.push_back(sig[j]);
    out.insert(canonical(std::move(c)));
  }

  // Promotions: replace a pawn with Q/R/B/N of the same colour.
  for (int i = 0; i < n; ++i)
  {
    if (type_of(sig[i]) != PAWN) continue;
    const Color col = color_of(sig[i]);
    for (PieceType pt : { QUEEN, ROOK, BISHOP, KNIGHT })
    {
      Sig c = sig;
      c[i] = make_piece(col, pt);
      out.insert(canonical(std::move(c)));
    }
  }
}

// ---- index <-> position --------------------------------------------------

uint64_t
numStates(int n)
{ return (uint64_t(1) << (6 * n)) * 2; }

// Decode a table index into per-slot squares (canonical order) + side to move.
void
decode(uint64_t idx, int n, std::array<int, 4>& sqs, Color& stm)
{
  stm = Color(idx & 1);
  idx >>= 1;
  for (int i = n - 1; i >= 0; --i)
  {
    sqs[i] = static_cast<int>(idx & 63);
    idx >>= 6;
  }
}

// Materialise the position for `sig` with slot i on sqs[i], side to move `stm`.
// Mirrors what FEN "... <stm> - - 0 1" would build (csep = 64 -> no castle/ep).
void
setupBoard(ChessBoard& pos, const Sig& sig, const std::array<int, 4>& sqs,
           int n, Color stm)
{
  pos.reset();
  for (int i = 0; i < n; ++i)
    pos.setPiece(Square(sqs[i]), sig[i]);
  pos.color = stm;
  pos.csep = 64;
}

// Geometry-only legality (no movegen): distinct squares, pawns on ranks 2-7,
// kings not adjacent. Catches the bulk of illegal index slots cheaply.
bool
geometryLegal(const Sig& sig, const std::array<int, 4>& sqs, int n)
{
  for (int i = 0; i < n; ++i)
    for (int j = i + 1; j < n; ++j)
      if (sqs[i] == sqs[j]) return false;

  int wk = -1, bk = -1;
  for (int i = 0; i < n; ++i)
  {
    if (type_of(sig[i]) == PAWN)
    {
      int r = sqs[i] >> 3;
      if (r == 0 || r == 7) return false;
    }
    if (type_of(sig[i]) == KING)
      (color_of(sig[i]) == WHITE ? wk : bk) = sqs[i];
  }
  if (wk >= 0 && bk >= 0 && kingDistance(wk, bk) <= 1) return false;
  return true;
}

// Read the men off a board into canonical slot order, filling `men[0..n)` with
// the Pieces and returning the table index. Men are sorted by (Piece, square):
// for distinct men the Piece key alone fixes the order; identical men are
// tie-broken by square so the lookup lands on the ascending-square
// representative the table stores. Allocation-free -- this is the hot path
// (called once per successor probe, billions of times during a solve).
uint64_t
indexMen(const ChessBoard& pos, std::array<Piece, 4>& men, int& n)
{
  std::array<std::pair<Piece, int>, 4> tmp{};
  n = 0;

  Bitboard bb = pos.all();
  while (bb)
  {
    int sq = __builtin_ctzll(bb);
    bb &= bb - 1;
    tmp[n++] = { pos.pieceOnSquare(Square(sq)), sq };
  }

  // Insertion sort by (Piece, square). Hand-rolled rather than std::sort: the
  // range is <= 4 elements, so introsort's machinery is pure overhead, and its
  // fixed 16-element final-insertion-sort step also makes GCC emit a bogus
  // -Warray-bounds on this `tmp[4]` (the >16 path is dead but not provably so).
  for (int i = 1; i < n; ++i)
  {
    std::pair<Piece, int> key = tmp[i];
    int j = i - 1;
    while (j >= 0 &&
           (tmp[j].first != key.first ? key.first < tmp[j].first
                                      : key.second < tmp[j].second))
    {
      tmp[j + 1] = tmp[j];
      --j;
    }
    tmp[j + 1] = key;
  }

  uint64_t idx = 0;
  for (int i = 0; i < n; ++i)
  {
    men[i] = tmp[i].first;
    idx = idx * 64 + static_cast<uint64_t>(tmp[i].second);
  }
  return idx * 2 + pos.color;
}

// Vector-returning convenience wrapper (used off the hot path, e.g. probe()).
uint64_t
indexOfBoard(const ChessBoard& pos, Sig& outSig)
{
  std::array<Piece, 4> men{};
  int n = 0;
  const uint64_t idx = indexMen(pos, men, n);
  outSig.assign(men.begin(), men.begin() + n);
  return idx;
}

// Does the canonical men span equal an already-canonical signature? (No alloc.)
bool
sameSig(const std::array<Piece, 4>& men, int n, const Sig& sig)
{
  if (static_cast<int>(sig.size()) != n) return false;
  for (int i = 0; i < n; ++i)
    if (men[i] != sig[i]) return false;
  return true;
}

// Is this the entry indexMen reads, i.e. are identical men in ascending-square
// order? Any other ordering (a "twin") is the same position under another index.
bool
isCanonicalSlot(const Sig& sig, const std::array<int, 4>& sqs, int n)
{
  for (int i = 1; i < n; ++i)
    if (sig[i] == sig[i - 1] && sqs[i] < sqs[i - 1]) return false;
  return true;
}

// Table index of `sig` with slot i on sqs[i], put in canonical order first: a
// backward move can carry one knight past its twin, and the index must then be
// the one indexMen would compute for that position.
uint64_t
slotIndex(const Sig& sig, std::array<int, 4> sqs, int n, Color stm)
{
  for (int i = 1; i < n; ++i)
    for (int j = i; j > 0 && sig[j] == sig[j - 1] && sqs[j] < sqs[j - 1]; --j)
      std::swap(sqs[j], sqs[j - 1]);

  uint64_t idx = 0;
  for (int i = 0; i < n; ++i)
    idx = idx * 64 + static_cast<uint64_t>(sqs[i]);
  return idx * 2 + stm;
}

// ---- backward move generation --------------------------------------------

// Squares a pawn of colour `c` now on `to` could have been pushed from: one rank
// back, or two from its start rank when both squares behind it are empty. Never
// from the back rank, where no pawn stands.
Bitboard
pawnOrigins(Color c, int to, Bitboard occ)
{
  const int back = (c == WHITE) ? -8 : 8;
  const int one  = to + back;
  if ((one >> 3) == 0 || (one >> 3) == 7 || ((occ >> one) & 1))
    return 0;

  Bitboard from = Bitboard(1) << one;
  const int two = one + back;
  const int startRank = (c == WHITE) ? 1 : 6;
  if ((two >> 3) == startRank && !((occ >> two) & 1))
    from |= Bitboard(1) << two;
  return from;
}

// Calls `emit(index)` for every entry of `sig` that reaches (sqs, stm) by one
// move that is neither a capture nor a promotion: the side that just moved takes
// back one move. Captures and promotions change the material, so those moves come
// from another table and need no undoing; castling and en passant do not occur.
//
// The list may hold entries that are illegal or already decided (the caller skips
// those by their value), but it must never miss a real predecessor;
// checkPredecessors tests that against the engine's own moves.
template <typename Emit>
void
forEachPredecessor(const Sig& sig, const std::array<int, 4>& sqs, int n,
                   Color stm, Emit&& emit)
{
  const Color mover = ~stm;
  Bitboard occ = 0;
  for (int i = 0; i < n; ++i)
    occ |= Bitboard(1) << sqs[i];

  for (int i = 0; i < n; ++i)
  {
    if (color_of(sig[i]) != mover) continue;

    // Piece moves are reversible: the man came from a square it attacks now.
    const Square to = Square(sqs[i]);
    Bitboard from = 0;
    switch (type_of(sig[i]))
    {
      case PAWN:   from = pawnOrigins(mover, to, occ);          break;
      case BISHOP: from = attackSquares<BISHOP>(to, occ) & ~occ; break;
      case KNIGHT: from = attackSquares<KNIGHT>(to, occ) & ~occ; break;
      case ROOK:   from = attackSquares<ROOK  >(to, occ) & ~occ; break;
      case QUEEN:  from = attackSquares<QUEEN >(to, occ) & ~occ; break;
      case KING:   from = attackSquares<KING  >(to, occ) & ~occ; break;
      default:     break;
    }

    std::array<int, 4> prev = sqs;
    while (from)
    {
      prev[i] = __builtin_ctzll(from);
      from &= from - 1;
      emit(slotIndex(sig, prev, n, mover));
    }
  }
}

} // namespace

// --------------------------------------------------------------------------

Wdl
EgSolver::valueOf(const ChessBoard& pos) const
{
  std::array<Piece, 4> men{};
  int n = 0;
  const uint64_t idx = indexMen(pos, men, n);

  if (insufficient(men.data(), n))
    return Wdl::DRAW;

  // Overwhelmingly common case: a quiet king/pawn move stays in the table we
  // are currently solving. Resolve it without ever building a Sig (no alloc).
  if (sameSig(men, n, currentSig))
    return (*currentTable)[idx];

  // Rare: a capture/promotion successor, resolved in an already-solved child
  // table. Only here do we pay for a Sig to key the registry.
  Sig sig(men.begin(), men.begin() + n);
  return registry.at(sig)[idx];
}

// One forward check of an undecided position: WIN if some move reaches a loss
// for the opponent, LOSS if every move reaches a win for the opponent, otherwise
// still UNKNOWN. Both solve methods decide positions only through this check.
Wdl
EgSolver::forwardValue(ChessBoard& pos) const
{
  const MoveList ml = generateMoves(pos);
  MoveArray moves;
  ml.getMoves(pos, moves);

  bool allWin = true;
  for (const Move mv : moves)
  {
    pos.makeMove(mv);
    const Wdl v = valueOf(pos);
    pos.unmakeMove();

    if (v == Wdl::LOSS) return Wdl::WIN;
    if (v != Wdl::WIN)  allWin = false;
  }
  return allWin ? Wdl::LOSS : Wdl::UNKNOWN;
}

void
EgSolver::solve(const Sig& sig, TableStats& stats)
{
  const int n = static_cast<int>(sig.size());
  const uint64_t total = numStates(n);

  std::vector<Wdl> table(total, Wdl::ILLEGAL);
  currentSig = sig;
  currentTable = &table;

  // Pass 1: classify illegal / terminal / interior(UNKNOWN). Every index is
  // independent, so this fans out across cores. Each thread keeps its own board
  // (its undo stack is a member, so distinct boards never alias) and writes only
  // its own slots.
  #pragma omp parallel
  {
    std::array<int, 4> sqs{};
    Color stm = WHITE;
    ChessBoard pos;

    #pragma omp for schedule(static)
    for (int64_t s = 0; s < static_cast<int64_t>(total); ++s)
    {
      const size_t si = static_cast<size_t>(s);
      decode(static_cast<uint64_t>(s), n, sqs, stm);
      if (!geometryLegal(sig, sqs, n)) continue;     // stays ILLEGAL

      setupBoard(pos, sig, sqs, n, stm);
      if (sideNotToMoveInCheck(pos, stm)) continue;  // stays ILLEGAL

      const MoveList ml = generateMoves(pos);
      if (!ml.anyMove())
        table[si] = stmInCheck(pos, stm) ? Wdl::LOSS : Wdl::DRAW;  // mate / stalemate
      else
        table[si] = Wdl::UNKNOWN;
    }
  }

  // Relaxation to a fixpoint: a node is WIN if any successor is a LOSS for the
  // opponent, LOSS if every successor is a WIN for the opponent; otherwise it
  // settles to DRAW. Values only ever flip UNKNOWN -> WIN/LOSS (monotone), and
  // the fixpoint does not depend on the order positions are checked in, so both
  // methods reach the same table.
  if (fullSweeps)
    relaxFullSweeps(sig, table, stats);
  else
    relaxFrontier(sig, table, stats);

  // Whatever is still undecided is a draw. The frontier method never checks
  // twins (identical men out of square order), so each one takes the value of
  // the entry it duplicates; with full sweeps the two already agree. A canonical
  // entry may turn from UNKNOWN to DRAW while its twin reads it, so an UNKNOWN
  // read also means DRAW.
  #pragma omp parallel
  {
    std::array<int, 4> sqs{};
    Color stm = WHITE;

    #pragma omp for schedule(static)
    for (int64_t s = 0; s < static_cast<int64_t>(total); ++s)
    {
      const size_t si = static_cast<size_t>(s);
      if (table[si] != Wdl::UNKNOWN) continue;

      decode(static_cast<uint64_t>(s), n, sqs, stm);
      Wdl v = Wdl::UNKNOWN;
      if (!isCanonicalSlot(sig, sqs, n))
        v = table[slotIndex(sig, sqs, n, stm)];
      table[si] = (v == Wdl::UNKNOWN) ? Wdl::DRAW : v;
    }
  }

  currentTable = nullptr;
  registry.emplace(sig, std::move(table));
}

// The original method: every sweep re-checks every undecided position, until a
// sweep decides nothing. The number of sweeps is about the longest win in the
// table, and the draws are re-checked on every one of them.
//
// Each sweep runs in parallel. The race on `table` is benign: a uint8 store is
// atomic on x86, and the only transitions are UNKNOWN -> WIN/LOSS, both final.
// A thread that reads a sibling's just-written WIN/LOSS merely converges
// faster; one that still reads UNKNOWN (treated as "not WIN, not LOSS") simply
// defers that node to a later sweep. A node is finalized LOSS only when *all*
// its successors are already WIN (final), so no node is ever decided wrongly --
// the fixpoint reached is bit-identical to the serial version. Each thread
// collects the indices it could not yet decide into a local list; those are
// merged to form the next sweep's worklist. n <= 4 => total < 2^26, so uint32
// indices suffice.
void
EgSolver::relaxFullSweeps(const Sig& sig, std::vector<Wdl>& table, TableStats& stats)
{
  const int n = static_cast<int>(sig.size());
  const uint64_t total = table.size();

  std::vector<uint32_t> unknown;
  for (uint64_t s = 0; s < total; ++s)
    if (table[s] == Wdl::UNKNOWN) unknown.push_back(static_cast<uint32_t>(s));

  bool changed = true;
  std::vector<uint32_t> nextUnknown;
  while (changed)
  {
    int changedFlag = 0;
    nextUnknown.clear();
    ++stats.sweeps;
    stats.evaluations += unknown.size();

    #pragma omp parallel
    {
      std::array<int, 4> sqs{};
      Color stm = WHITE;
      ChessBoard pos;
      std::vector<uint32_t> localKeep;
      int localChanged = 0;

      #pragma omp for schedule(dynamic, 4096) nowait
      for (int64_t r = 0; r < static_cast<int64_t>(unknown.size()); ++r)
      {
        const uint32_t s = unknown[static_cast<size_t>(r)];
        decode(s, n, sqs, stm);
        setupBoard(pos, sig, sqs, n, stm);

        const Wdl v = forwardValue(pos);
        if (v != Wdl::UNKNOWN) { table[s] = v; localChanged = 1; }
        else                   localKeep.push_back(s);   // still undecided -> keep
      }

      #pragma omp critical
      {
        nextUnknown.insert(nextUnknown.end(), localKeep.begin(), localKeep.end());
        changedFlag |= localChanged;
      }
    }

    changed = changedFlag != 0;
    unknown.swap(nextUnknown);
  }
}

// The default method: the first sweep checks every undecided position, and each
// later sweep checks only the predecessors of positions decided in the sweep
// before, since nothing else can have changed. (The first sweep must be full: a
// win through a capture or promotion has no predecessor in this table to
// trigger it.) The backward generator only chooses what to check; every decision
// still comes from forwardValue, so an extra predecessor costs one check and
// changes nothing.
//
// The positions to check are kept as bitsets over the table, one bit per entry,
// so duplicates collapse for free and each sweep walks the table in index order.
// Each word of `cur` is read and cleared by one thread; `next` is set by many,
// hence the atomic OR. The race on `table` is the benign one described above:
// a position decided during a sweep is a predecessor's trigger for the next
// sweep, whether or not the predecessor already saw it in this one.
void
EgSolver::relaxFrontier(const Sig& sig, std::vector<Wdl>& table, TableStats& stats)
{
  const int n = static_cast<int>(sig.size());
  const uint64_t total = table.size();
  const int64_t words = static_cast<int64_t>(total / 64);   // total = 2^(6n+1)

  std::vector<uint64_t> cur(static_cast<size_t>(words), 0);
  std::vector<uint64_t> next(static_cast<size_t>(words), 0);

  // Sweep 1: every undecided canonical entry. Twins are filled in after the solve.
  #pragma omp parallel
  {
    std::array<int, 4> sqs{};
    Color stm = WHITE;

    #pragma omp for schedule(static)
    for (int64_t w = 0; w < words; ++w)
    {
      uint64_t bits = 0;
      for (int b = 0; b < 64; ++b)
      {
        const uint64_t s = static_cast<uint64_t>(w) * 64 + static_cast<uint64_t>(b);
        if (table[s] != Wdl::UNKNOWN) continue;
        decode(s, n, sqs, stm);
        if (isCanonicalSlot(sig, sqs, n)) bits |= uint64_t(1) << b;
      }
      cur[static_cast<size_t>(w)] = bits;
    }
  }

  for (;;)
  {
    uint64_t checked = 0, decided = 0;

    #pragma omp parallel
    {
      std::array<int, 4> sqs{};
      Color stm = WHITE;
      ChessBoard pos;

      #pragma omp for schedule(dynamic, 64) reduction(+ : checked, decided)
      for (int64_t w = 0; w < words; ++w)
      {
        uint64_t bits = cur[static_cast<size_t>(w)];
        if (!bits) continue;
        cur[static_cast<size_t>(w)] = 0;

        while (bits)
        {
          const uint64_t s = static_cast<uint64_t>(w) * 64
                           + static_cast<uint64_t>(__builtin_ctzll(bits));
          bits &= bits - 1;
          if (table[s] != Wdl::UNKNOWN) continue;

          decode(s, n, sqs, stm);
          setupBoard(pos, sig, sqs, n, stm);
          ++checked;

          const Wdl v = forwardValue(pos);
          if (v == Wdl::UNKNOWN) continue;
          table[s] = v;
          ++decided;

          forEachPredecessor(sig, sqs, n, stm, [&] (uint64_t p)
          {
            if (table[p] != Wdl::UNKNOWN) return;
            uint64_t* word = &next[p >> 6];
            const uint64_t mask = uint64_t(1) << (p & 63);
            if (!(__atomic_load_n(word, __ATOMIC_RELAXED) & mask))
              __atomic_fetch_or(word, mask, __ATOMIC_RELAXED);
          });
        }
      }
    }

    ++stats.sweeps;
    stats.evaluations += checked;
    if (decided == 0) break;
    cur.swap(next);   // `cur` was cleared word by word, so `next` starts empty
  }
}

bool
EgSolver::build(const std::vector<Piece>& extras, std::string& err)
{
  Sig target = { make_piece(WHITE, KING), make_piece(BLACK, KING) };
  for (Piece p : extras) target.push_back(p);
  target = canonical(std::move(target));

  if (target.size() > 4)
  {
    err = "oracle supports at most 4 men (5-man needs symmetry reduction)";
    return false;
  }

  // Discover the full DAG of signatures that must be solved.
  std::set<Sig> seen;
  std::vector<Sig> needed;
  std::queue<Sig> q;
  q.push(target);
  while (!q.empty())
  {
    Sig s = q.front();
    q.pop();
    if (seen.count(s)) continue;
    seen.insert(s);
    if (insufficient(s)) continue;     // constant draw, no table
    needed.push_back(s);

    std::set<Sig> kids;
    childSignatures(s, kids);
    for (const Sig& k : kids) q.push(k);
  }

  // Bottom-up order: fewer men first, and pawnless before pawnful at equal
  // count (a pawn only leaves its table by promoting -> a pawnless table of the
  // same count, or being captured -> a smaller table; both solved earlier).
  std::sort(needed.begin(), needed.end(),
            [] (const Sig& a, const Sig& b)
            {
              if (a.size() != b.size()) return a.size() < b.size();
              return pawnCount(a) < pawnCount(b);
            });

  // Solve bottom-up. Each table is a pure function of move generation, so a
  // cached copy on disk is reloaded verbatim (sub-second) instead of recomputed;
  // a miss solves and then persists for next time. Children are cached by their
  // own signature, so they are shared across any target that reaches them.
  tablesSolved = tablesLoaded = 0;
  lastBuild.clear();
  for (const Sig& s : needed)
  {
    TableStats stats;
    stats.sig = s;
    const bool tryLoad = solveTargetOnly ? s != target : cacheEnabled;
    if (tryLoad && cacheLoad(s))
    {
      stats.loaded = true;
      ++tablesLoaded;
    }
    else
    {
      const perf_clock start = perf::now();
      solve(s, stats);
      stats.seconds = perf_time(perf::now() - start).count();
      if (cacheEnabled && !solveTargetOnly) cacheSave(s);
      ++tablesSolved;
    }
    lastBuild.push_back(std::move(stats));
  }

  return true;
}

void
EgSolver::checkPredecessors(const Sig& sig, uint64_t& edges, uint64_t& missing,
                            std::string& example) const
{
  const int n = static_cast<int>(sig.size());
  const uint64_t total = numStates(n);
  uint64_t e = 0, m = 0;
  example.clear();

  #pragma omp parallel
  {
    std::array<int, 4> sqs{}, nextSqs{};
    Color stm = WHITE, nextStm = WHITE;
    std::array<Piece, 4> men{};
    int menCount = 0;
    ChessBoard pos;

    #pragma omp for schedule(dynamic, 4096) reduction(+ : e, m)
    for (int64_t s = 0; s < static_cast<int64_t>(total); ++s)
    {
      decode(static_cast<uint64_t>(s), n, sqs, stm);
      if (!isCanonicalSlot(sig, sqs, n) || !geometryLegal(sig, sqs, n)) continue;
      setupBoard(pos, sig, sqs, n, stm);
      if (sideNotToMoveInCheck(pos, stm)) continue;

      const MoveList ml = generateMoves(pos);
      MoveArray moves;
      ml.getMoves(pos, moves);

      for (const Move mv : moves)
      {
        pos.makeMove(mv);
        const uint64_t q = indexMen(pos, men, menCount);
        const bool stays = sameSig(men, menCount, sig);
        pos.unmakeMove();
        if (!stays) continue;   // a capture or promotion: another table

        ++e;
        decode(q, n, nextSqs, nextStm);
        bool found = false;
        forEachPredecessor(sig, nextSqs, n, nextStm,
                           [&] (uint64_t p) { if (p == static_cast<uint64_t>(s)) found = true; });
        if (found) continue;

        ++m;
        #pragma omp critical
        {
          if (example.empty())
            example = pos.fen() + "  move " + printMove(mv, pos);
        }
      }
    }
  }

  edges = e;
  missing = m;
}

Wdl
EgSolver::probe(const ChessBoard& pos) const
{
  Sig sig;
  const uint64_t idx = indexOfBoard(pos, sig);

  if (insufficient(sig))
    return Wdl::DRAW;

  const auto it = registry.find(sig);
  if (it == registry.end())
    return Wdl::ILLEGAL;   // signature not solved (shouldn't happen for built target)
  return it->second[idx];
}

bool
EgSolver::distribution(const std::vector<Piece>& extras,
                       uint64_t& win, uint64_t& draw, uint64_t& loss) const
{
  Sig sig = { make_piece(WHITE, KING), make_piece(BLACK, KING) };
  for (Piece p : extras) sig.push_back(p);
  sig = canonical(std::move(sig));

  const auto it = registry.find(sig);
  if (it == registry.end()) return false;

  win = draw = loss = 0;
  for (Wdl v : it->second)
  {
    if (v == Wdl::WIN)       ++win;
    else if (v == Wdl::LOSS) ++loss;
    else if (v == Wdl::DRAW) ++draw;
  }
  return true;
}

// --------------------------------------------------------------------------
// Disk persistence
// --------------------------------------------------------------------------

std::string
EgSolver::resolvedCacheDir() const
{
  const std::filesystem::path base = cacheDir;
  if (base.is_relative())
  {
    const std::filesystem::path ed = exeDir();
    if (!ed.empty())
      return (ed / base).string();
  }
  return cacheDir;   // absolute, or exe path unknown -> resolves CWD-relative
}

std::string
EgSolver::cachePath(const Sig& sig) const
{
  if (cacheDir.empty() || sig.size() > 4)
    return {};

  // Key = the canonical signature's raw Piece bytes in hex. Bytes (not FEN
  // letters) because NTFS is case-insensitive: 'P' and 'p' would collide.
  static const char hex[] = "0123456789abcdef";
  std::string key;
  for (Piece p : sig)
  {
    key += hex[(static_cast<uint8_t>(p) >> 4) & 0xF];
    key += hex[ static_cast<uint8_t>(p)       & 0xF];
  }
  return resolvedCacheDir() + "/sig_" + key + ".wdl";
}

bool
EgSolver::cacheLoad(const Sig& sig)
{
  std::vector<Wdl> table;
  if (!readCacheFile(sig, table))
    return false;
  registry.emplace(sig, std::move(table));
  return true;
}

bool
EgSolver::compareWithCache(const Sig& sig, uint64_t& differing) const
{
  const auto it = registry.find(sig);
  std::vector<Wdl> cached;
  if (it == registry.end() || !readCacheFile(sig, cached))
    return false;

  differing = 0;
  for (size_t i = 0; i < cached.size(); ++i)
    if (cached[i] != it->second[i]) ++differing;
  return true;
}

bool
EgSolver::readCacheFile(const Sig& sig, std::vector<Wdl>& table) const
{
  const std::string path = cachePath(sig);
  if (path.empty())
    return false;

  std::ifstream in(path, std::ios::binary);
  if (!in)
    return false;

  CacheHeader h{};
  if (!in.read(reinterpret_cast<char*>(&h), sizeof h))
    return false;

  // Validate every field: a mismatch (old format, stale solver, key collision,
  // truncated file) is treated as a miss so we re-solve rather than trust it.
  if (std::memcmp(h.magic, CACHE_MAGIC, sizeof h.magic) != 0) return false;
  if (h.solverVer != SOLVER_VERSION)                          return false;
  if (h.n != sig.size())                                      return false;
  for (size_t i = 0; i < sig.size(); ++i)
    if (h.pieces[i] != static_cast<uint32_t>(sig[i]))         return false;
  if (h.total != numStates(static_cast<int>(sig.size())))     return false;

  table.assign(h.total, Wdl::ILLEGAL);
  if (!in.read(reinterpret_cast<char*>(table.data()),
               static_cast<std::streamsize>(h.total)))
    return false;
  // Reject a file with trailing junk (size must be exactly header + table).
  return in.peek() == std::ifstream::traits_type::eof();
}

void
EgSolver::cacheSave(const Sig& sig)
{
  const std::string path = cachePath(sig);
  if (path.empty())
    return;

  const auto it = registry.find(sig);
  if (it == registry.end())
    return;
  const std::vector<Wdl>& table = it->second;

  std::error_code ec;
  std::filesystem::create_directories(resolvedCacheDir(), ec);   // best effort

  CacheHeader h{};
  std::memcpy(h.magic, CACHE_MAGIC, sizeof h.magic);
  h.solverVer = SOLVER_VERSION;
  h.n         = static_cast<uint32_t>(sig.size());
  for (size_t i = 0; i < sig.size() && i < 4; ++i)
    h.pieces[i] = static_cast<uint32_t>(sig[i]);
  h.total = table.size();

  // Atomic publish: write a temp file, then rename over the final path so a
  // crash mid-write never leaves a truncated table that later loads as garbage.
  const std::string tmp = path + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return;
    out.write(reinterpret_cast<const char*>(&h), sizeof h);
    out.write(reinterpret_cast<const char*>(table.data()),
              static_cast<std::streamsize>(table.size()));
    if (!out) { out.close(); std::filesystem::remove(tmp, ec); return; }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec)   // some platforms won't rename over an existing file
  {
    std::filesystem::remove(path, ec);
    std::filesystem::rename(tmp, path, ec);
    if (ec) std::filesystem::remove(tmp, ec);
  }
}
