
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

using Sig    = EgSolver::Sig;
using Layout = EgSolver::Layout;

namespace {

constexpr int MAX_MEN = EgSolver::MAX_MEN;

// Square of each slot's man, in slot order.
using Squares = std::array<int, MAX_MEN>;

// On-disk cache format. MAGIC tags the layout; SOLVER_VERSION tags the *meaning*
// of the bytes and MUST be bumped whenever the solver, move generation, or
// legality test changes, so stale tables from an older engine are rejected
// rather than silently trusted. Either mismatch => the file is ignored and the
// table is re-solved. (Escape hatch for an un-versioned change: `nocache`.)
constexpr char     CACHE_MAGIC[4]   = { 'E', 'G', 'W', '2' };
constexpr uint32_t SOLVER_VERSION   = 4;

// Fixed-size header prefixed to the packed table (EgSolver::Table). Every field
// is re-validated on load (incl. the signature itself and the trailing byte
// count), so a collision or truncated/partial file can never feed the oracle
// wrong data.
struct CacheHeader
{
  char     magic[4];
  uint32_t solverVer;
  uint32_t n;                // men count
  uint32_t pieces[MAX_MEN];  // the canonical signature (NO_PIECE-padded)
  uint64_t total;            // == the layout's total; (total + 3) / 4 bytes follow
};

// The format before 2026-10-05, read only by compareWithOldCache: no symmetry,
// slot i's square in 6 bits each, then the side to move (64^n * 2 entries).
constexpr char     OLD_CACHE_MAGIC[4] = { 'E', 'G', 'W', '1' };
constexpr uint32_t OLD_SOLVER_VERSION = 1;

struct OldCacheHeader
{
  char     magic[4];
  uint32_t solverVer;
  uint32_t n;
  uint32_t pieces[4];
  uint64_t total;
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

int fileOf(int sq) { return sq & 7; }
int rankOf(int sq) { return sq >> 3; }

int
kingDistance(int a, int b)
{
  return std::max(std::abs(rankOf(a) - rankOf(b)), std::abs(fileOf(a) - fileOf(b)));
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

// ---- symmetry ------------------------------------------------------------
//
// A frame is one of the 8 symmetries of the board: bit 0 flips the files
// (a <-> h), bit 1 the ranks (1 <-> 8), and bit 2 then reflects in the a1-h8
// diagonal. Without pawns or castling all 8 keep the rules of chess, so a
// position and its images have the same value; with pawns only the left-right
// mirror (frames 0 and 1) does.
constexpr int FLIP_FILES = 1, FLIP_RANKS = 2, TRANSPOSE = 4;

// King pairs in the region (Layout): 462 without pawns, 1806 with them.
constexpr int MAX_KING_PAIRS = 1806;

struct IndexTables
{
  uint8_t  sym[8][64];                    // square in frame t
  uint8_t  frame[2][64];                  // [pawns][white king square]: frame that puts the king in the region
  int16_t  kingPair[2][64][64];           // [pawns][wk][bk]: number of the pair, -1 if not in the region
  uint8_t  pairSquares[2][MAX_KING_PAIRS][2];
  int      pairCount[2];
  uint64_t binom[65][MAX_MEN + 1];        // binom[s][k] = C(s, k)

  IndexTables()
  {
    for (int t = 0; t < 8; ++t)
      for (int sq = 0; sq < 64; ++sq)
      {
        int s = sq;
        if (t & FLIP_FILES) s ^= 7;
        if (t & FLIP_RANKS) s ^= 56;
        if (t & TRANSPOSE)  s = (fileOf(s) << 3) | rankOf(s);
        sym[t][sq] = static_cast<uint8_t>(s);
      }

    for (int sq = 0; sq < 64; ++sq)
    {
      int f = fileOf(sq), r = rankOf(sq), t = 0;
      if (f > 3) { t |= FLIP_FILES; f = 7 - f; }
      frame[1][sq] = static_cast<uint8_t>(t);     // files a-d
      if (r > 3) { t |= FLIP_RANKS; r = 7 - r; }
      if (r > f) t |= TRANSPOSE;
      frame[0][sq] = static_cast<uint8_t>(t);     // the triangle a1-d1-d4
    }

    // A white king in the region and a black king not next to it. Without pawns
    // a white king on the diagonal leaves the transpose free, so the black king
    // is put on or below the diagonal.
    for (int p = 0; p < 2; ++p)
    {
      int count = 0;
      for (int wk = 0; wk < 64; ++wk)
        for (int bk = 0; bk < 64; ++bk)
        {
          kingPair[p][wk][bk] = -1;
          if (frame[p][wk] != 0 || kingDistance(wk, bk) <= 1) continue;
          if (p == 0 && fileOf(wk) == rankOf(wk) && rankOf(bk) > fileOf(bk)) continue;
          kingPair[p][wk][bk] = static_cast<int16_t>(count);
          pairSquares[p][count][0] = static_cast<uint8_t>(wk);
          pairSquares[p][count][1] = static_cast<uint8_t>(bk);
          ++count;
        }
      pairCount[p] = count;
    }

    for (int s = 0; s <= 64; ++s)
      for (int k = 0; k <= MAX_MEN; ++k)
        binom[s][k] = (k == 0) ? 1 : (s == 0) ? 0 : binom[s - 1][k - 1] + binom[s - 1][k];
  }
};

const IndexTables tables;

// ---- index <-> position --------------------------------------------------

Layout
makeLayout(const Sig& sig)
{
  Layout layout;
  layout.n = static_cast<int>(sig.size());
  layout.pawns = pawnCount(sig) > 0;

  for (int i = 0; i < layout.n; ++i)
  {
    if (sig[i] == make_piece(WHITE, KING)) { layout.wk = i; continue; }
    if (sig[i] == make_piece(BLACK, KING)) { layout.bk = i; continue; }
    if (i > 0 && sig[i] == sig[i - 1])     { ++layout.count[layout.groups - 1]; continue; }

    layout.first [layout.groups] = i;
    layout.count [layout.groups] = 1;
    layout.offset[layout.groups] = (type_of(sig[i]) == PAWN) ? 8 : 0;
    ++layout.groups;
  }

  layout.total = static_cast<uint64_t>(tables.pairCount[layout.pawns]);
  for (int g = 0; g < layout.groups; ++g)
  {
    layout.size[g] = tables.binom[layout.offset[g] ? 48 : 64][layout.count[g]];
    layout.total *= layout.size[g];
  }
  layout.total *= 2;
  return layout;
}

// Entry of the position (slot i's man on sqs[i]) seen in frame t, which must
// put the kings on a pair in the region. A run of identical men is numbered by
// its squares in ascending order, s0 < s1 < ...: C(s0,1) + C(s1,2) + ...
uint64_t
indexIn(const Layout& layout, const Squares& sqs, int t, Color stm)
{
  const uint8_t* sym = tables.sym[t];
  uint64_t idx = static_cast<uint64_t>(tables.kingPair[layout.pawns][sym[sqs[layout.wk]]][sym[sqs[layout.bk]]]);

  for (int g = 0; g < layout.groups; ++g)
  {
    const int f = layout.first[g], c = layout.count[g];
    uint64_t v = 0;
    if (c == 1)
      v = static_cast<uint64_t>(sym[sqs[f]] - layout.offset[g]);
    else
    {
      Squares s{};
      for (int j = 0; j < c; ++j)
      {
        const int x = sym[sqs[f + j]] - layout.offset[g];
        int k = j;
        for (; k > 0 && s[k - 1] > x; --k) s[k] = s[k - 1];
        s[k] = x;
      }
      for (int j = 0; j < c; ++j)
        v += tables.binom[s[j]][j + 1];
    }
    idx = idx * layout.size[g] + v;
  }
  return idx * 2 + static_cast<uint64_t>(stm);
}

// Table entry of a legal position (slot i's man on sqs[i]): the frame is the
// one that puts the white king in the region. Without pawns, a white king on
// the diagonal leaves the transpose free: it puts the black king below the
// diagonal, and when both kings are on it, the position and its reflection both
// fit and the smaller entry is used. The larger one is never read and stays
// ILLEGAL.
uint64_t
indexOf(const Layout& layout, const Squares& sqs, Color stm)
{
  int t = tables.frame[layout.pawns][sqs[layout.wk]];
  if (!layout.pawns)
  {
    const int wk = tables.sym[t][sqs[layout.wk]];
    const int bk = tables.sym[t][sqs[layout.bk]];
    if (fileOf(wk) == rankOf(wk))
    {
      if (rankOf(bk) > fileOf(bk))
        t ^= TRANSPOSE;
      else if (rankOf(bk) == fileOf(bk))
        return std::min(indexIn(layout, sqs, t, stm), indexIn(layout, sqs, t ^ TRANSPOSE, stm));
    }
  }
  return indexIn(layout, sqs, t, stm);
}

// Decode a table entry into per-slot squares (in the region's frame, identical
// men in ascending order) + side to move. The men may overlap; the caller
// checks geometryLegal.
void
decodeIndex(const Layout& layout, uint64_t idx, Squares& sqs, Color& stm)
{
  stm = Color(idx & 1);
  idx >>= 1;
  for (int g = layout.groups - 1; g >= 0; --g)
  {
    uint64_t v = idx % layout.size[g];
    idx /= layout.size[g];
    const int f = layout.first[g], c = layout.count[g], off = layout.offset[g];
    if (c == 1)
    {
      sqs[f] = static_cast<int>(v) + off;
      continue;
    }
    int x = (off ? 48 : 64) - 1;
    for (int j = c; j >= 1; --j)
    {
      while (tables.binom[x][j] > v) --x;
      v -= tables.binom[x][j];
      sqs[f + j - 1] = x + off;
      --x;
    }
  }
  sqs[layout.wk] = tables.pairSquares[layout.pawns][idx][0];
  sqs[layout.bk] = tables.pairSquares[layout.pawns][idx][1];
}

// Pack a solved table, in which no entry is UNKNOWN any more, at 2 bits per
// entry (see EgSolver::Table).
std::vector<uint8_t>
pack(const std::vector<Wdl>& wdl)
{
  const int64_t bytes = static_cast<int64_t>((wdl.size() + 3) / 4);
  std::vector<uint8_t> packed(static_cast<size_t>(bytes), 0);

  #pragma omp parallel for schedule(static)
  for (int64_t b = 0; b < bytes; ++b)
  {
    unsigned byte = 0;
    for (unsigned j = 0; j < 4; ++j)
    {
      const size_t i = static_cast<size_t>(b) * 4 + j;
      if (i < wdl.size() && wdl[i] != Wdl::ILLEGAL)
        byte |= (static_cast<unsigned>(wdl[i]) - 1) << (j * 2);
    }
    packed[static_cast<size_t>(b)] = static_cast<uint8_t>(byte);
  }
  return packed;
}

// Materialise the position for `sig` with slot i on sqs[i], side to move `stm`,
// no castling and no en passant square (csep = 64). Unlike a FEN-built board,
// the material weight and the hash stay zero and the move number keeps
// reset()'s value; move generation and the legality checks don't read them.
void
setupBoard(ChessBoard& pos, const Sig& sig, const Squares& sqs, int n, Color stm)
{
  pos.reset();
  for (int i = 0; i < n; ++i)
    pos.setPiece(Square(sqs[i]), sig[i]);
  pos.color = stm;
  pos.csep = 64;
}

// Geometry-only legality (no movegen): distinct squares, pawns on ranks 2-7,
// kings not adjacent. Catches the bulk of illegal index slots cheaply. Core
// takes a raw men span so probe() can avoid a Sig alloc.
bool
geometryLegal(const Piece* men, const Squares& sqs, int n)
{
  for (int i = 0; i < n; ++i)
    for (int j = i + 1; j < n; ++j)
      if (sqs[i] == sqs[j]) return false;

  int wk = -1, bk = -1;
  for (int i = 0; i < n; ++i)
  {
    if (type_of(men[i]) == PAWN)
    {
      int r = rankOf(sqs[i]);
      if (r == 0 || r == 7) return false;
    }
    if (type_of(men[i]) == KING)
      (color_of(men[i]) == WHITE ? wk : bk) = sqs[i];
  }
  if (wk >= 0 && bk >= 0 && kingDistance(wk, bk) <= 1) return false;
  return true;
}

bool
geometryLegal(const Sig& sig, const Squares& sqs, int n)
{ return geometryLegal(sig.data(), sqs, n); }

// Read the men off a board into slot order, sorted by Piece; squares come out
// of the bitboard in ascending order and the sort is stable, so identical men
// end up by square. Returns the number of men, which the caller guarantees is
// at most MAX_MEN. Allocation-free -- this is the hot path (called once per
// successor probe, billions of times during a solve).
int
readMen(const ChessBoard& pos, std::array<Piece, MAX_MEN>& men, Squares& sqs)
{
  int n = 0;
  Bitboard bb = pos.all();
  while (bb)
  {
    const int sq = __builtin_ctzll(bb);
    bb &= bb - 1;
    const Piece p = pos.pieceOnSquare(Square(sq));
    int j = n++;
    for (; j > 0 && men[j - 1] > p; --j)
    {
      men[j] = men[j - 1];
      sqs[j] = sqs[j - 1];
    }
    men[j] = p;
    sqs[j] = sq;
  }
  return n;
}

// Does the canonical men span equal an already-canonical signature? (No alloc.)
bool
sameSig(const std::array<Piece, MAX_MEN>& men, int n, const Sig& sig)
{
  if (static_cast<int>(sig.size()) != n) return false;
  for (int i = 0; i < n; ++i)
    if (men[i] != sig[i]) return false;
  return true;
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
  if (rankOf(one) == 0 || rankOf(one) == 7 || ((occ >> one) & 1))
    return 0;

  Bitboard from = Bitboard(1) << one;
  const int two = one + back;
  const int startRank = (c == WHITE) ? 1 : 6;
  if (rankOf(two) == startRank && !((occ >> two) & 1))
    from |= Bitboard(1) << two;
  return from;
}

// Calls `emit(entry)` for every entry of `sig` that reaches (sqs, stm) by one
// move that is neither a capture nor a promotion: the side that just moved takes
// back one move. Captures and promotions change the material, so those moves come
// from another table and need no undoing; en passant is a capture, and castling
// does not occur.
// A position that reaches a mirror image of (sqs, stm) is the mirror image of
// one that reaches (sqs, stm) itself, and both have the same entry.
//
// The list may hold entries that are illegal or already decided (the caller skips
// those by their value), but it must never miss a real predecessor;
// checkPredecessors tests that against the engine's own moves. Every position it
// builds is geometry-legal, which indexOf needs: men move only to empty squares,
// pawns never to the back rank, and a king never next to the other king.
template <typename Emit>
void
forEachPredecessor(const Sig& sig, const Layout& layout, const Squares& sqs,
                   Color stm, Emit&& emit)
{
  const Color mover = ~stm;
  const int n = layout.n;
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
      case KING:
      {
        const Square other = Square(sqs[mover == WHITE ? layout.bk : layout.wk]);
        from = attackSquares<KING>(to, occ) & ~occ & ~attackSquares<KING>(other, occ);
        break;
      }
      default:     break;
    }

    Squares prev = sqs;
    while (from)
    {
      prev[i] = __builtin_ctzll(from);
      from &= from - 1;
      emit(indexOf(layout, prev, mover));
    }
  }
}

} // namespace

// --------------------------------------------------------------------------

// Value of a position whose signature is `sig`, whose entries `own(i)` gives,
// or that of a solved table.
template <typename Own>
Wdl
EgSolver::valueIn(const ChessBoard& pos, const Sig& sig, const Layout& layout,
                  const Own& own) const
{
  std::array<Piece, MAX_MEN> men{};
  Squares sqs{};
  const int n = readMen(pos, men, sqs);

  if (insufficient(men.data(), n))
    return Wdl::DRAW;

  // Overwhelmingly common case: a quiet move stays in the given table. Resolve
  // it without ever building a Sig (no alloc).
  Wdl v;
  if (sameSig(men, n, sig))
    v = own(indexOf(layout, sqs, pos.color));
  else
  {
    // Rare: a capture/promotion successor, resolved in an already-solved child
    // table. Only here do we pay for a Sig to key the registry.
    const Table& t = registry.at(Sig(men.begin(), men.begin() + n));
    v = t.at(indexOf(t.layout, sqs, pos.color));
  }
  return pos.enPassantSquare() == SQUARE_NB ? v : enPassantValue(pos, v);
}

// Value of a position with an en passant square, given its entry `v`, which is
// its value without one. The capture leads into a smaller, solved table, whose
// entry is ILLEGAL when the capture leaves the capturer's king in check. The
// position takes the better of `v` and its legal captures, except that one whose
// only moves are those captures (stalemate without them) takes theirs alone.
Wdl
EgSolver::enPassantValue(const ChessBoard& pos, Wdl v) const
{
  const Color stm = pos.color;
  const int ep = pos.enPassantSquare();
  const int victim = ep + (stm == WHITE ? -8 : 8);
  if (v == Wdl::ILLEGAL || pos.pieceOnSquare(Square(victim)) != make_piece(~stm, PAWN))
    return v;

  std::array<Piece, MAX_MEN> men{};
  Squares sqs{};
  const int n = readMen(pos, men, sqs);

  bool any = false;
  Wdl best = Wdl::LOSS;
  for (Bitboard from = plt::pawnCaptureMasks[~stm][ep] & pos.getPiece(stm, PAWN);
       from; from &= from - 1)
  {
    // The men after the capture, still in slot order: the captured pawn leaves
    // and the capturer keeps its place.
    const int capturer = __builtin_ctzll(from);
    std::array<Piece, MAX_MEN> kid{};
    Squares kidSqs{};
    int k = 0;
    for (int i = 0; i < n; ++i)
    {
      if (sqs[i] == victim) continue;
      kid[k] = men[i];
      kidSqs[k++] = (sqs[i] == capturer) ? ep : sqs[i];
    }

    Wdl after = Wdl::DRAW;   // the capturer is still a pawn, so never insufficient
    if (!insufficient(kid.data(), k))
    {
      const Table& t = registry.at(Sig(kid.begin(), kid.begin() + k));
      after = t.at(indexOf(t.layout, kidSqs, ~stm));
    }
    if (after == Wdl::ILLEGAL) continue;

    any = true;
    best = std::max(best, after == Wdl::WIN ? Wdl::LOSS : after == Wdl::LOSS ? Wdl::WIN : Wdl::DRAW);
  }

  if (!any)                return v;
  if (best == Wdl::WIN)    return Wdl::WIN;
  if (v == Wdl::UNKNOWN)   return Wdl::UNKNOWN;   // still being solved
  if (v == Wdl::DRAW && best == Wdl::LOSS)
  {
    const MoveList ml = generateMoves(pos);
    MoveArray moves;
    ml.getMoves(pos, moves);
    bool otherMove = false;
    for (const Move mv : moves)
      if (to_sq(mv) != ep || type_of(pos.pieceOnSquare(from_sq(mv))) != PAWN)
        otherMove = true;
    if (!otherMove) return Wdl::LOSS;
  }
  return std::max(v, best);
}

Wdl
EgSolver::valueOf(const ChessBoard& pos) const
{
  const std::vector<Wdl>& table = *currentTable;
  return valueIn(pos, currentSig, currentLayout, [&] (uint64_t i) { return table[i]; });
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
  const Layout layout = makeLayout(sig);
  const int n = layout.n;
  const uint64_t total = layout.total;

  std::vector<Wdl> table(total, Wdl::ILLEGAL);
  currentSig = sig;
  currentLayout = layout;
  currentTable = &table;

  // Pass 1: classify illegal / terminal / interior(UNKNOWN). Every index is
  // independent, so this fans out across cores. Each thread keeps its own board
  // (its undo stack is a member, so distinct boards never alias) and writes only
  // its own slots. The unused reflection of a position with both kings on the
  // diagonal (see indexOf) stays ILLEGAL, so each position is checked once.
  #pragma omp parallel
  {
    Squares sqs{};
    Color stm = WHITE;
    ChessBoard pos;

    #pragma omp for schedule(static)
    for (int64_t s = 0; s < static_cast<int64_t>(total); ++s)
    {
      const uint64_t si = static_cast<uint64_t>(s);
      decodeIndex(layout, si, sqs, stm);
      if (!geometryLegal(sig, sqs, n)) continue;     // stays ILLEGAL
      if (indexOf(layout, sqs, stm) != si) continue;      // stays ILLEGAL

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
    relaxFullSweeps(sig, layout, table, stats);
  else
    relaxFrontier(sig, layout, table, stats);

  // Whatever is still undecided is a draw.
  #pragma omp parallel for schedule(static)
  for (int64_t s = 0; s < static_cast<int64_t>(total); ++s)
    if (table[static_cast<size_t>(s)] == Wdl::UNKNOWN)
      table[static_cast<size_t>(s)] = Wdl::DRAW;

  currentTable = nullptr;
  registry.emplace(sig, Table{ layout, pack(table) });
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
// merged to form the next sweep's worklist. Every table up to 5 men has fewer
// than 2^32 entries, so uint32 indices suffice.
void
EgSolver::relaxFullSweeps(const Sig& sig, const Layout& layout, std::vector<Wdl>& table,
                          TableStats& stats)
{
  const int n = layout.n;
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
      Squares sqs{};
      Color stm = WHITE;
      ChessBoard pos;
      std::vector<uint32_t> localKeep;
      int localChanged = 0;

      #pragma omp for schedule(dynamic, 4096) nowait
      for (int64_t r = 0; r < static_cast<int64_t>(unknown.size()); ++r)
      {
        const uint32_t s = unknown[static_cast<size_t>(r)];
        decodeIndex(layout, s, sqs, stm);
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
EgSolver::relaxFrontier(const Sig& sig, const Layout& layout, std::vector<Wdl>& table,
                        TableStats& stats)
{
  const int n = layout.n;
  const uint64_t total = table.size();
  const int64_t words = static_cast<int64_t>((total + 63) / 64);

  std::vector<uint64_t> cur(static_cast<size_t>(words), 0);
  std::vector<uint64_t> next(static_cast<size_t>(words), 0);

  // Sweep 1: every undecided entry.
  #pragma omp parallel for schedule(static)
  for (int64_t w = 0; w < words; ++w)
  {
    uint64_t bits = 0;
    for (uint64_t b = 0; b < 64; ++b)
    {
      const uint64_t s = static_cast<uint64_t>(w) * 64 + b;
      if (s < total && table[s] == Wdl::UNKNOWN) bits |= uint64_t(1) << b;
    }
    cur[static_cast<size_t>(w)] = bits;
  }

  for (;;)
  {
    uint64_t checked = 0, decided = 0;

    #pragma omp parallel
    {
      Squares sqs{};
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

          decodeIndex(layout, s, sqs, stm);
          setupBoard(pos, sig, sqs, n, stm);
          ++checked;

          const Wdl v = forwardValue(pos);
          if (v == Wdl::UNKNOWN) continue;
          table[s] = v;
          ++decided;

          forEachPredecessor(sig, layout, sqs, stm, [&] (uint64_t p)
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

  if (target.size() > MAX_MEN)
  {
    err = "oracle supports at most " + std::to_string(MAX_MEN) + " men";
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
    stats.entries = makeLayout(s).total;
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

  targetSig = target;
  const auto it = registry.find(target);
  targetTable = (it == registry.end()) ? nullptr : &it->second;   // none if insufficient
  return true;
}

void
EgSolver::checkPredecessors(const Sig& sig, uint64_t& edges, uint64_t& missing,
                            std::string& example) const
{
  const Layout layout = makeLayout(sig);
  const int n = layout.n;
  uint64_t e = 0, m = 0;
  example.clear();

  #pragma omp parallel
  {
    Squares sqs{}, nextSqs{};
    Color stm = WHITE, nextStm = WHITE;
    std::array<Piece, MAX_MEN> men{};
    ChessBoard pos;

    #pragma omp for schedule(dynamic, 4096) reduction(+ : e, m)
    for (int64_t s = 0; s < static_cast<int64_t>(layout.total); ++s)
    {
      const uint64_t si = static_cast<uint64_t>(s);
      decodeIndex(layout, si, sqs, stm);
      if (!geometryLegal(sig, sqs, n) || indexOf(layout, sqs, stm) != si) continue;
      setupBoard(pos, sig, sqs, n, stm);
      if (sideNotToMoveInCheck(pos, stm)) continue;

      const MoveList ml = generateMoves(pos);
      MoveArray moves;
      ml.getMoves(pos, moves);

      for (const Move mv : moves)
      {
        pos.makeMove(mv);
        const int menCount = readMen(pos, men, nextSqs);
        const bool stays = sameSig(men, menCount, sig);
        const uint64_t q = stays ? indexOf(layout, nextSqs, pos.color) : 0;
        pos.unmakeMove();
        if (!stays) continue;   // a capture or promotion: another table

        ++e;
        decodeIndex(layout, q, nextSqs, nextStm);
        bool found = false;
        forEachPredecessor(sig, layout, nextSqs, nextStm,
                           [&] (uint64_t p) { if (p == si) found = true; });
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

bool
EgSolver::verify(const Sig& sig, uint64_t& positions, uint64_t& wrong,
                 std::string& example) const
{
  const auto it = registry.find(sig);
  if (it == registry.end())
    return false;

  const Table& table = it->second;
  const Layout& layout = table.layout;
  const auto own = [&] (uint64_t i) { return table.at(i); };
  const int n = layout.n;
  uint64_t p = 0, w = 0;
  example.clear();

  // Value of the position a move reaches. One with an en passant square has no
  // entry of its own; it is valued by its moves, as the engine generates them,
  // rather than through enPassantValue, so that function is checked too.
  const auto reached = [&] (const auto& self, ChessBoard& pos) -> Wdl
  {
    if (pos.enPassantSquare() == SQUARE_NB)
      return valueIn(pos, sig, layout, own);

    const MoveList ml = generateMoves(pos);
    if (!ml.anyMove())
      return stmInCheck(pos, pos.color) ? Wdl::LOSS : Wdl::DRAW;
    MoveArray moves;
    ml.getMoves(pos, moves);
    bool allWin = true;
    for (const Move mv : moves)
    {
      pos.makeMove(mv);
      const Wdl v = self(self, pos);
      pos.unmakeMove();
      if (v == Wdl::LOSS) return Wdl::WIN;
      if (v != Wdl::WIN)  allWin = false;
    }
    return allWin ? Wdl::LOSS : Wdl::DRAW;
  };

  #pragma omp parallel
  {
    Squares sqs{};
    Color stm = WHITE;
    ChessBoard pos;

    #pragma omp for schedule(dynamic, 4096) reduction(+ : p, w)
    for (int64_t s = 0; s < static_cast<int64_t>(layout.total); ++s)
    {
      const uint64_t si = static_cast<uint64_t>(s);
      decodeIndex(layout, si, sqs, stm);

      Wdl expected = Wdl::ILLEGAL;
      if (geometryLegal(sig, sqs, n) && indexOf(layout, sqs, stm) == si)
      {
        setupBoard(pos, sig, sqs, n, stm);
        if (!sideNotToMoveInCheck(pos, stm))
        {
          ++p;
          const MoveList ml = generateMoves(pos);
          MoveArray moves;
          ml.getMoves(pos, moves);

          bool win = false, allWin = true;
          for (const Move mv : moves)
          {
            pos.makeMove(mv);
            const Wdl v = reached(reached, pos);
            pos.unmakeMove();
            if (v == Wdl::LOSS) { win = true; break; }
            if (v != Wdl::WIN)  allWin = false;
          }

          if (!ml.anyMove())
            expected = stmInCheck(pos, stm) ? Wdl::LOSS : Wdl::DRAW;   // mate / stalemate
          else
            expected = win ? Wdl::WIN : allWin ? Wdl::LOSS : Wdl::DRAW;
        }
      }

      const Wdl stored = table.at(si);
      if (stored == expected)
        continue;
      ++w;
      #pragma omp critical
      {
        if (example.empty())
        {
          static const char* const name[] = { "illegal", "unknown", "loss", "draw", "win" };
          example = "entry " + std::to_string(si);
          if (geometryLegal(sig, sqs, n))
          {
            setupBoard(pos, sig, sqs, n, stm);
            example += " (" + pos.fen() + ")";
          }
          example += std::string(" holds ") + name[static_cast<int>(stored)]
                   + ", expected " + name[static_cast<int>(expected)];
        }
      }
    }
  }

  positions = p;
  wrong = w;
  return true;
}

Wdl
EgSolver::probe(const ChessBoard& pos) const
{
  if (__builtin_popcountll(pos.all()) > MAX_MEN)
    return Wdl::ILLEGAL;   // no such table

  std::array<Piece, MAX_MEN> men{};
  Squares sqs{};
  const int n = readMen(pos, men, sqs);

  if (insufficient(men.data(), n))
    return Wdl::DRAW;
  if (!geometryLegal(men.data(), sqs, n))
    return Wdl::ILLEGAL;   // no entry for adjacent kings

  // The built target is found without a Sig or a registry search: egvalidate
  // probes it once per generated position.
  const Table* t = (targetTable && sameSig(men, n, targetSig)) ? targetTable : nullptr;
  if (!t)
  {
    const auto it = registry.find(Sig(men.begin(), men.begin() + n));
    if (it == registry.end())
      return Wdl::ILLEGAL;   // signature not solved (shouldn't happen for built target)
    t = &it->second;
  }
  const Wdl v = t->at(indexOf(t->layout, sqs, pos.color));
  return pos.enPassantSquare() == SQUARE_NB ? v : enPassantValue(pos, v);
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
  const Table& t = it->second;
  for (uint64_t i = 0; i < t.layout.total; ++i)
  {
    const Wdl v = t.at(i);
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

namespace {

// Key = the canonical signature's raw Piece bytes in hex. Bytes (not FEN
// letters) because NTFS is case-insensitive: 'P' and 'p' would collide.
std::string
sigKey(const Sig& sig)
{
  static const char hex[] = "0123456789abcdef";
  std::string key;
  for (Piece p : sig)
  {
    key += hex[(static_cast<uint8_t>(p) >> 4) & 0xF];
    key += hex[ static_cast<uint8_t>(p)       & 0xF];
  }
  return key;
}

} // namespace

std::string
EgSolver::cachePath(const Sig& sig) const
{
  if (cacheDir.empty() || sig.size() > MAX_MEN)
    return {};
  return resolvedCacheDir() + "/sig_" + sigKey(sig) + ".wdl2";
}

std::string
EgSolver::oldCachePath(const Sig& sig) const
{
  if (cacheDir.empty() || sig.size() > 4)
    return {};
  return resolvedCacheDir() + "/sig_" + sigKey(sig) + ".wdl";
}

bool
EgSolver::cacheLoad(const Sig& sig)
{
  std::vector<uint8_t> packed;
  if (!readCacheFile(sig, packed))
    return false;
  registry.emplace(sig, Table{ makeLayout(sig), std::move(packed) });
  return true;
}

bool
EgSolver::compareWithCache(const Sig& sig, uint64_t& differing) const
{
  const auto it = registry.find(sig);
  Table cached{ it == registry.end() ? Layout{} : it->second.layout, {} };
  if (it == registry.end() || !readCacheFile(sig, cached.packed))
    return false;

  differing = 0;
  for (uint64_t i = 0; i < cached.layout.total; ++i)
    if (cached.at(i) != it->second.at(i)) ++differing;
  return true;
}

bool
EgSolver::compareWithOldCache(const Sig& sig, uint64_t& differing) const
{
  const auto it = registry.find(sig);
  const std::string path = oldCachePath(sig);
  if (it == registry.end() || path.empty())
    return false;

  std::ifstream in(path, std::ios::binary);
  OldCacheHeader h{};
  if (!in || !in.read(reinterpret_cast<char*>(&h), sizeof h))
    return false;

  const int n = static_cast<int>(sig.size());
  if (std::memcmp(h.magic, OLD_CACHE_MAGIC, sizeof h.magic) != 0) return false;
  if (h.solverVer != OLD_SOLVER_VERSION)                          return false;
  if (h.n != sig.size())                                          return false;
  for (int i = 0; i < n; ++i)
    if (h.pieces[i] != static_cast<uint32_t>(sig[i]))             return false;
  if (h.total != (uint64_t(1) << (6 * n)) * 2)                    return false;

  std::vector<Wdl> old(h.total);
  if (!in.read(reinterpret_cast<char*>(old.data()), static_cast<std::streamsize>(h.total)))
    return false;

  const Table& table = it->second;
  const Layout& layout = table.layout;
  uint64_t d = 0;

  // Every legal position there has the same value here.
  #pragma omp parallel for schedule(static) reduction(+ : d)
  for (int64_t s = 0; s < static_cast<int64_t>(h.total); ++s)
  {
    const Wdl v = old[static_cast<size_t>(s)];
    if (v == Wdl::ILLEGAL) continue;

    Squares sqs{};
    uint64_t idx = static_cast<uint64_t>(s);
    const Color stm = Color(idx & 1);
    idx >>= 1;
    for (int i = n - 1; i >= 0; --i, idx >>= 6)
      sqs[i] = static_cast<int>(idx & 63);
    if (table.at(indexOf(layout, sqs, stm)) != v) ++d;
  }

  // Every entry here is a legal position there.
  #pragma omp parallel for schedule(static) reduction(+ : d)
  for (int64_t s = 0; s < static_cast<int64_t>(layout.total); ++s)
  {
    if (table.at(static_cast<uint64_t>(s)) == Wdl::ILLEGAL) continue;

    Squares sqs{};
    Color stm = WHITE;
    decodeIndex(layout, static_cast<uint64_t>(s), sqs, stm);
    uint64_t idx = 0;
    for (int i = 0; i < n; ++i)
      idx = idx * 64 + static_cast<uint64_t>(sqs[i]);
    if (old[idx * 2 + static_cast<uint64_t>(stm)] == Wdl::ILLEGAL) ++d;
  }

  differing = d;
  return true;
}

bool
EgSolver::readCacheFile(const Sig& sig, std::vector<uint8_t>& packed) const
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
  if (h.total != makeLayout(sig).total)                       return false;

  packed.assign((h.total + 3) / 4, 0);
  if (!in.read(reinterpret_cast<char*>(packed.data()),
               static_cast<std::streamsize>(packed.size())))
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
  const Table& table = it->second;

  std::error_code ec;
  std::filesystem::create_directories(resolvedCacheDir(), ec);   // best effort

  CacheHeader h{};
  std::memcpy(h.magic, CACHE_MAGIC, sizeof h.magic);
  h.solverVer = SOLVER_VERSION;
  h.n         = static_cast<uint32_t>(sig.size());
  for (size_t i = 0; i < sig.size() && i < MAX_MEN; ++i)
    h.pieces[i] = static_cast<uint32_t>(sig[i]);
  h.total = table.layout.total;

  // Atomic publish: write a temp file, then rename over the final path so a
  // crash mid-write never leaves a truncated table that later loads as garbage.
  const std::string tmp = path + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return;
    out.write(reinterpret_cast<const char*>(&h), sizeof h);
    out.write(reinterpret_cast<const char*>(table.packed.data()),
              static_cast<std::streamsize>(table.packed.size()));
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
