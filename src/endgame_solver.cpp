
#include <algorithm>
#include <array>
#include <queue>
#include <set>

#include "endgame_solver.h"
#include "movegen.h"
#include "move_utils.h"

using Sig = EgSolver::Sig;

namespace {

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

  std::sort(tmp.begin(), tmp.begin() + n,
            [] (const auto& a, const auto& b)
            { return a.first != b.first ? a.first < b.first : a.second < b.second; });

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

void
EgSolver::solve(const Sig& sig)
{
  const int n = static_cast<int>(sig.size());
  const uint64_t total = numStates(n);

  std::vector<Wdl> table(total, Wdl::ILLEGAL);
  currentSig = sig;
  currentTable = &table;

  // Pass 1: classify illegal / terminal / interior(UNKNOWN). Every index is
  // independent, so this fans out across cores. Each thread keeps its own board
  // (its undo stack is a member, so distinct boards never alias) and writes only
  // its own slots; the worklist of UNKNOWN indices is gathered in a cheap serial
  // scan afterwards. n <= 4 => total < 2^26, so uint32 indices suffice.
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

  std::vector<uint32_t> unknown;
  for (uint64_t s = 0; s < total; ++s)
    if (table[s] == Wdl::UNKNOWN) unknown.push_back(static_cast<uint32_t>(s));

  // Relaxation to a fixpoint: a node is WIN if any successor is a LOSS for the
  // opponent, LOSS if every successor is a WIN for the opponent; otherwise it
  // settles to DRAW. Values only ever flip UNKNOWN -> WIN/LOSS (monotone), so
  // this converges in at most (max distance-to-mate) sweeps.
  //
  // Each sweep runs in parallel. The race on `table` is benign: a uint8 store is
  // atomic on x86, and the only transitions are UNKNOWN -> WIN/LOSS, both final.
  // A thread that reads a sibling's just-written WIN/LOSS merely converges
  // faster; one that still reads UNKNOWN (treated as "not WIN, not LOSS") simply
  // defers that node to a later sweep. A node is finalized LOSS only when *all*
  // its successors are already WIN (final), so no node is ever decided wrongly --
  // the fixpoint reached is bit-identical to the serial version. Each thread
  // collects the indices it could not yet decide into a local list; those are
  // merged to form the next sweep's worklist.
  bool changed = true;
  std::vector<uint32_t> nextUnknown;
  while (changed)
  {
    int changedFlag = 0;
    nextUnknown.clear();

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

        const MoveList ml = generateMoves(pos);
        MoveArray moves;
        ml.getMoves(pos, moves);

        bool anyLoss = false, allWin = true;
        for (const Move mv : moves)
        {
          pos.makeMove(mv);
          const Wdl v = valueOf(pos);
          pos.unmakeMove();

          if (v == Wdl::LOSS) { anyLoss = true; break; }
          if (v != Wdl::WIN)  allWin = false;
        }

        if (anyLoss)      { table[s] = Wdl::WIN;  localChanged = 1; }
        else if (allWin)  { table[s] = Wdl::LOSS; localChanged = 1; }
        else              localKeep.push_back(s);   // still undecided -> keep
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

  for (uint32_t s : unknown)
    table[s] = Wdl::DRAW;

  currentTable = nullptr;
  registry.emplace(sig, std::move(table));
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

  for (const Sig& s : needed)
    solve(s);

  return true;
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
