
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <array>
#include <vector>
#include <memory>
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "endgame_validation.h"
#include "endgame_solver.h"
#include "bitboard.h"
#include "endgame.h"
#include "movegen.h"
#include "move_utils.h"
#include "base_utils.h"
#include "perf.h"

using std::string;
using std::vector;
using std::cout;

namespace {

// One extra man (beyond the two kings) to place on the board.
struct Slot
{
  char fenChar;   // 'P', 'b', 'R', 'n', ... (case encodes colour)
  bool isPawn;    // pawns are restricted to ranks 2-7
};

// Chebyshev (king) distance between two 0..63 square indices.
int
kingDistance(int a, int b)
{
  int r1 = a >> 3, f1 = a & 7;
  int r2 = b >> 3, f2 = b & 7;
  return std::max(std::abs(r1 - r2), std::abs(f1 - f2));
}

// Is the side that is NOT to move sitting in check? That position is illegal
// (it would have been the other side's move). Reuses the engine's own attack
// detection so this matches exactly what search would compute.
bool
sideNotToMoveInCheck(const ChessBoard& pos, Color stm)
{
  return (stm == WHITE) ? inCheck<BLACK>(pos) : inCheck<WHITE>(pos);
}

// Map a piece letter to (fenChar, isPawn); returns false for kings / unknowns.
bool
parsePiece(char c, Slot& out)
{
  switch (std::toupper(static_cast<unsigned char>(c)))
  {
    case 'P': case 'N': case 'B': case 'R': case 'Q':
      out.fenChar = c;
      out.isPawn = (std::toupper(static_cast<unsigned char>(c)) == 'P');
      return true;
    default:
      return false; // 'K'/'k' (kings are implicit) and anything else
  }
}

// Map a piece letter (case = colour) to the engine's Piece encoding.
Piece
charToPiece(char c)
{
  const bool white = std::isupper(static_cast<unsigned char>(c)) != 0;
  PieceType pt = NONE;
  switch (std::toupper(static_cast<unsigned char>(c)))
  {
    case 'P': pt = PAWN;   break;
    case 'N': pt = KNIGHT; break;
    case 'B': pt = BISHOP; break;
    case 'R': pt = ROOK;   break;
    case 'Q': pt = QUEEN;  break;
    default:  pt = NONE;   break;
  }
  return make_piece(white ? WHITE : BLACK, pt);
}

// Flip the colour of every piece letter (P<->p): turns one colouring of a
// material into its colour-mirror (e.g. "Pb" -> "pB").
string
flipCase(const string& s)
{
  string r = s;
  for (char& c : r)
    c = std::isupper(static_cast<unsigned char>(c))
          ? static_cast<char>(std::tolower(static_cast<unsigned char>(c)))
          : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return r;
}

// Do two piece strings name the same multiset of men? If so, a colouring is its
// own colour-mirror (e.g. "Pp") and there is no distinct second colouring.
bool
sameMaterial(string a, string b)
{
  std::sort(a.begin(), a.end());
  std::sort(b.begin(), b.end());
  return a == b;
}

// Human-readable signature for an extras string, e.g. "Pb" -> "KPKB".
string
signatureOf(const string& extras)
{
  string w, b;
  for (char c : extras)
  {
    char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    (std::isupper(static_cast<unsigned char>(c)) ? w : b) += u;
  }
  return "K" + w + "K" + b;
}

// Parse an extras string (kings excluded) into Slots. On an invalid letter,
// returns false and sets badChar.
bool
parseExtras(const string& s, vector<Slot>& extras, char& badChar)
{
  extras.clear();
  for (char c : s)
  {
    Slot sl;
    if (!parsePiece(c, sl)) { badChar = c; return false; }
    extras.push_back(sl);
  }
  return true;
}

// Exhaustive generator over a fixed piece set (the two kings plus `extras`).
//
// Symmetry: the position is folded by vertical mirror, canonicalised on the
// WHITE KING's file (restricted to a-d). This is valid for any material -- the
// game is left-right symmetric with no castling/en-passant in play -- and since
// there is exactly one white king and no central file, every mirror orbit has
// size exactly 2, so folding the white king to a-d picks each orbit's
// representative once: no orbit dropped, none double-counted.
constexpr int    WK = 0;          // slot index of the white king
constexpr int    BK = 1;          // slot index of the black king
constexpr size_t MAX_FALSE_FENS = 64;

// All tallies for one colouring. Each worker accumulates its own and they are
// summed at the end; summation is order-independent so the parallel total equals
// the serial total exactly.
struct Tally
{
  // Enumeration counters.
  uint64_t geom = 0, rejInCheck = 0;
  uint64_t legal = 0, legalW = 0, legalB = 0;
  uint64_t rejTerminal = 0, rejCaptures = 0;
  uint64_t quiet = 0, quietW = 0, quietB = 0;
  uint64_t heurDraw = 0, heurNonDraw = 0, heurDrawW = 0, heurDrawB = 0;

  // Oracle scorecard (only when an oracle is present). The 4-bucket confusion
  // matrix of isTheoreticalDraw vs perfect WDL, plus the oracle's own W/D/L
  // split over the call set. `falseDraw` is the dangerous bucket.
  uint64_t agreeDraw = 0, agreeNondraw = 0, missedDraw = 0, falseDraw = 0;
  uint64_t oWin = 0, oDraw = 0, oLoss = 0, oBad = 0;

  Tally&
  operator+=(const Tally& o)
  {
    geom += o.geom; rejInCheck += o.rejInCheck;
    legal += o.legal; legalW += o.legalW; legalB += o.legalB;
    rejTerminal += o.rejTerminal; rejCaptures += o.rejCaptures;
    quiet += o.quiet; quietW += o.quietW; quietB += o.quietB;
    heurDraw += o.heurDraw; heurNonDraw += o.heurNonDraw;
    heurDrawW += o.heurDrawW; heurDrawB += o.heurDrawB;
    agreeDraw += o.agreeDraw; agreeNondraw += o.agreeNondraw;
    missedDraw += o.missedDraw; falseDraw += o.falseDraw;
    oWin += o.oWin; oDraw += o.oDraw; oLoss += o.oLoss; oBad += o.oBad;
    return *this;
  }
};

// One worker enumerates the subtree under a single fixed (side-to-move, white
// king square) and writes only its own Tally / false-FEN list / dump buffer.
// All the shared state it touches is read-only: the slot layout, the magic
// attack tables (read by generateMoves), the pure isTheoreticalDraw, and the
// solved oracle (probe() is const). So many workers run with no locking, and
// the per-task slices are disjoint -- summing reproduces the serial result.
struct Walker
{
  const vector<Slot>*       slots    = nullptr;
  const std::array<int,16>* prevSame = nullptr;  // nearest earlier same-fenChar slot, or -1
  int                       maxKingFile = 3;     // 3 = folded (a-d); 7 = allfiles self-check
  const EgSolver*           oracle   = nullptr;
  bool                      wantDump = false;

  Color stm = WHITE;
  std::array<int, 16> square{};   // current square per slot

  Tally               t;
  std::vector<string> falseFens;  // up to MAX_FALSE_FENS examples for this slice
  string              dump;       // dump lines for this slice (merged in task order)

  string
  buildFen() const
  {
    char sq[64] = {0};
    for (int i = 0; i < static_cast<int>(slots->size()); ++i)
      sq[square[i]] = (*slots)[i].fenChar;

    string fen;
    for (int r = 7; r >= 0; --r)
    {
      int empty = 0;
      for (int f = 0; f < 8; ++f)
      {
        char c = sq[r * 8 + f];
        if (c == 0)
          ++empty;
        else
        {
          if (empty) { fen += char('0' + empty); empty = 0; }
          fen += c;
        }
      }
      if (empty) fen += char('0' + empty);
      if (r) fen += '/';
    }

    fen += (stm == WHITE) ? " w - - 0 1" : " b - - 0 1";
    return fen;
  }

  void
  leaf()
  {
    ++t.geom;
    const string fen = buildFen();
    ChessBoard pos(fen);

    if (sideNotToMoveInCheck(pos, stm))
    { ++t.rejInCheck; return; }

    ++t.legal;
    (stm == WHITE ? t.legalW : t.legalB)++;

    // Match the engine's gate exactly: isTheoreticalDraw is consulted only on
    // non-terminal positions with no captures available for the side to move
    // (single_thread.cpp:51, :331). Positions the search would never hand to the
    // recognizer must not pollute the tally.
    const MoveList moves = generateMoves(pos);
    if (!moves.anyMove())
    { ++t.rejTerminal; return; }            // checkmate / stalemate
    if (moves.exists<MType::CAPTURES>(pos))
    { ++t.rejCaptures; return; }            // a capture is available

    ++t.quiet;
    (stm == WHITE ? t.quietW : t.quietB)++;

    const bool isDraw = isTheoreticalDraw(pos);
    if (isDraw)
    {
      ++t.heurDraw;
      (stm == WHITE ? t.heurDrawW : t.heurDrawB)++;
    }
    else
      ++t.heurNonDraw;

    // Oracle verdict (side-to-move relative) as a single char, also appended to
    // the dump so a position can be filtered by truth: 'W'/'L'/'D', '?' = bad.
    char oracleCh = 0;
    if (oracle)
    {
      const Wdl truth = oracle->probe(pos);
      const bool oracleDraw = (truth == Wdl::DRAW);

      if      (truth == Wdl::WIN)  { ++t.oWin;  oracleCh = 'W'; }
      else if (truth == Wdl::LOSS) { ++t.oLoss; oracleCh = 'L'; }
      else if (truth == Wdl::DRAW) { ++t.oDraw; oracleCh = 'D'; }
      else                         { ++t.oBad;  oracleCh = '?'; }  // ILLEGAL/UNKNOWN -- should never happen

      if (isDraw && oracleDraw)        ++t.agreeDraw;
      else if (!isDraw && !oracleDraw) ++t.agreeNondraw;
      else if (!isDraw && oracleDraw)  ++t.missedDraw;   // safe coverage gap
      else                                               // heuristic draw, truth decided
      {
        ++t.falseDraw;                                   // DANGEROUS
        if (falseFens.size() < MAX_FALSE_FENS)
          falseFens.push_back(fen);
      }
    }

    // dump: `FEN | <D|.>` (recognizer) and, when an oracle is present,
    // ` | <W|D|L>` (truth). A missed-draw line is exactly `| . | D`.
    if (wantDump)
    {
      dump += fen;
      dump += " | ";
      dump += (isDraw ? 'D' : '.');
      if (oracle) { dump += " | "; dump += oracleCh; }
      dump += '\n';
    }
  }

  // Place the man for `slot`, then recurse. Cheap geometric rejects (file fold,
  // pawn rank, overlap, king adjacency) prune whole subtrees before the
  // expensive FEN-build + parse + in-check test at the leaf. The white king
  // (slot WK) is pre-placed by the caller, so workers enter at place(BK).
  void
  place(int slot)
  {
    if (slot == static_cast<int>(slots->size()))
    { leaf(); return; }

    const Slot& sp = (*slots)[slot];

    for (int sq = 0; sq < 64; ++sq)
    {
      // Vertical-mirror fold: white king canonicalised to files a-d.
      if (slot == WK && (sq & 7) > maxKingFile)
        continue;

      // Pawns live on ranks 2-7 (rank index 1..6) only.
      if (sp.isPawn)
      {
        int r = sq >> 3;
        if (r < 1 || r > 6) continue;
      }

      // Distinct squares.
      bool overlap = false;
      for (int j = 0; j < slot; ++j)
        if (square[j] == sq) { overlap = true; break; }
      if (overlap) continue;

      // Identical men (same fenChar -> same colour and type) are
      // interchangeable: placing them independently would generate each
      // position k! times. Enforce a strict ascending-square order across
      // identical slots so every multiset placement is emitted exactly once.
      // (square[prevSame] is already set: prevSame < slot, placed earlier.)
      if ((*prevSame)[slot] >= 0 && sq <= square[(*prevSame)[slot]])
        continue;

      // Kings never adjacent (checked the moment the black king lands).
      if (slot == BK && kingDistance(square[WK], sq) <= 1)
        continue;

      square[slot] = sq;
      place(slot + 1);
    }
  }
};

// Drives the enumeration for one colouring: owns the slot layout and the merged
// results, and fans the work out across (side-to-move, white-king square) tasks.
struct Generator
{
  vector<Slot> slots;          // index 0 = white king, 1 = black king, then extras
  int maxKingFile = 3;         // 3 = folded (a-d); 7 = allfiles self-check
  bool wantDump = false;
  const EgSolver* oracle = nullptr;

  std::array<int, 16> prevSame{}; // nearest earlier slot with same fenChar, or -1

  // Merged results (summed / concatenated from the workers in task order).
  Tally               t;
  std::vector<string> falseFens;
  string              dump;

  // Precompute, for each slot, the nearest earlier slot carrying the same
  // fenChar (or -1). Drives the identical-piece ordering constraint in place().
  void
  computePrevSame()
  {
    for (int i = 0; i < static_cast<int>(slots.size()); ++i)
    {
      prevSame[i] = -1;
      for (int j = i - 1; j >= 0; --j)
        if (slots[j].fenChar == slots[i].fenChar) { prevSame[i] = j; break; }
    }
  }

  // Enumerate both sides to move in parallel. Each (stm, white-king square) is an
  // independent task over a disjoint slice of the space, so the per-task tallies
  // sum to the serial total exactly; merging the false-FEN lists and dump buffers
  // in task order (WHITE before BLACK, king square ascending) reproduces the
  // serial *ordering* too -- the parallel run is bit-identical to the serial one.
  void
  run()
  {
    computePrevSame();

    struct Task { Color stm; int wkSq; };
    vector<Task> tasks;
    for (Color c : {WHITE, BLACK})
      for (int sq = 0; sq < 64; ++sq)
        if ((sq & 7) <= maxKingFile)        // honour the white-king file fold
          tasks.push_back({c, sq});

    const int n = static_cast<int>(tasks.size());
    vector<Walker> workers(static_cast<size_t>(n));

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < n; ++i)
    {
      Walker& w   = workers[static_cast<size_t>(i)];
      w.slots     = &slots;
      w.prevSame  = &prevSame;
      w.maxKingFile = maxKingFile;
      w.oracle    = oracle;
      w.wantDump  = wantDump;
      w.stm       = tasks[static_cast<size_t>(i)].stm;
      w.square[WK] = tasks[static_cast<size_t>(i)].wkSq;
      w.place(BK);                          // white king fixed; recurse from black king down
    }

    // Reduce in task order. Sums are order-independent, but draining the false
    // FENs and dump in order keeps the output deterministic regardless of how
    // the threads were scheduled.
    for (int i = 0; i < n; ++i)
    {
      Walker& w = workers[static_cast<size_t>(i)];
      t += w.t;
      for (const string& f : w.falseFens)
        if (falseFens.size() < MAX_FALSE_FENS)
          falseFens.push_back(f);
      if (wantDump)
        dump += w.dump;
    }
  }
};

// Print one colouring's tally block (no header / footer).
void
reportColouring(const Generator& g, const string& pieceStr)
{
  cout << "--- Colouring " << signatureOf(pieceStr)
       << " (pieces " << pieceStr << ") ---\n";

  cout << "Geometric placements    : " << g.t.geom
       << "  (distinct sq, kings >= 2 apart, pawns on 2-7)\n";
  cout << "  rejected (in check)   : " << g.t.rejInCheck
       << "  (side not to move)\n";
  cout << "Legal positions         : " << g.t.legal
       << "  (stm White " << g.t.legalW
       << ", stm Black " << g.t.legalB << ")\n";
  cout << "  rejected (terminal)   : " << g.t.rejTerminal
       << "  (checkmate / stalemate -- no moves)\n";
  cout << "  rejected (has capture): " << g.t.rejCaptures
       << "  (search skips the recognizer here)\n";
  cout << "Recognizer call set     : " << g.t.quiet
       << "  (stm White " << g.t.quietW
       << ", stm Black " << g.t.quietB << ")\n";

  const auto pct = [&] (uint64_t n) {
    return g.t.quiet
      ? (100.0 * static_cast<double>(n) / static_cast<double>(g.t.quiet))
      : 0.0;
  };

  cout << std::fixed << std::setprecision(2);
  cout << "  isTheoreticalDraw true  : " << g.t.heurDraw
       << "  (" << pct(g.t.heurDraw) << "% of call set)"
       << "  [stm W " << g.t.heurDrawW
       << ", B " << g.t.heurDrawB << "]\n";
  cout << "  isTheoreticalDraw false : " << g.t.heurNonDraw
       << "  (" << pct(g.t.heurNonDraw) << "% of call set)\n\n";
}

// Print the oracle confusion matrix for one colouring: isTheoreticalDraw's
// boolean label vs the solver's perfect WDL, collapsed to draw-vs-decided.
void
reportScorecard(const Generator& g, const string& pieceStr)
{
  const uint64_t total = g.t.agreeDraw + g.t.agreeNondraw + g.t.missedDraw + g.t.falseDraw;

  cout << "--- Oracle scorecard " << signatureOf(pieceStr)
       << " (pieces " << pieceStr << ") ---\n";
  cout << "Call-set positions scored : " << total << '\n';
  cout << "Oracle WDL (side-to-move)  : win " << g.t.oWin
       << ", draw " << g.t.oDraw << ", loss " << g.t.oLoss;
  if (g.t.oBad)
    cout << "  [** " << g.t.oBad << " unresolved -- BUG **]";
  cout << "\n\n";

  cout << "Confusion matrix (isTheoreticalDraw vs perfect WDL):\n";
  cout << "  agree-draw    (heur draw,  truth draw)    : " << g.t.agreeDraw    << "  correct\n";
  cout << "  agree-nondraw (heur false, truth decided) : " << g.t.agreeNondraw << "  correct (defers to eval)\n";
  cout << "  missed-draw   (heur false, truth draw)    : " << g.t.missedDraw   << "  safe gap\n";
  cout << "  FALSE-DRAW    (heur draw,  truth decided) : " << g.t.falseDraw
       << (g.t.falseDraw ? "  ** DANGEROUS **" : "  (none -- good)") << '\n';

  const double agreePct = total
    ? 100.0 * static_cast<double>(g.t.agreeDraw + g.t.agreeNondraw) / static_cast<double>(total)
    : 0.0;
  cout << std::fixed << std::setprecision(2)
       << "  agreement                                 : " << agreePct << "%\n";

  if (!g.falseFens.empty())
  {
    cout << "  false-draw FENs (first " << g.falseFens.size() << "):\n";
    for (const string& f : g.falseFens)
      cout << "    " << f << '\n';
  }
  cout << '\n';
}

} // namespace

void
validateEndgame(const vector<string>& args)
{
  // elsa egvalidate [pieces <set>] [oracle] [threads <n>] [mirror] [nocache] [allfiles] [dump <file>]
  //
  // Exhaustively enumerate every legal position for a material signature -- the
  // two kings (always present, never passed) plus the extra men named by
  // `pieces` -- and tally isTheoreticalDraw over the recognizer's call set
  // (legal, non-terminal, no capture available; see single_thread.cpp:51,331).
  //
  //   pieces P    -> white pawn                  (KPK)
  //   pieces Pb   -> white pawn + black bishop    (KPKB, the default)
  //   pieces Rn   -> white rook + black knight    (KRKN)
  // Case encodes colour: UPPER = white, lower = black. Kings are implicit.
  //
  // By default only the named colouring is enumerated. `mirror` adds the
  // colour-mirror (e.g. Pb and pB), related by colour-swap + rank-flip: a
  // correct (colour-symmetric) recognizer must give identical call-set and draw
  // counts for the two, so the pair is a built-in colour-symmetry self-check.
  // The oracle build is per-colouring, so `mirror` roughly doubles the runtime;
  // a self-mirror material (e.g. Pp) has only one colouring regardless.
  //
  // `allfiles` disables the white-king fold (each tally must then double).
  //
  // The oracle's solved tables are cached under output/egcache/ (keyed by
  // signature), so the first build pays the full solve and later runs on the
  // same/overlapping material load in well under a second. `nocache` forces a
  // fresh solve and skips persisting -- use it after a movegen change that did
  // not bump the cache's SOLVER_VERSION.

  const string pieceArg = utils::hasArg(args, "pieces")
                        ? utils::argValue(args, "pieces")
                        : string("Pb");

  vector<Slot> extras;
  char badChar = 0;
  if (!parseExtras(pieceArg, extras, badChar))
  {
    cout << "Invalid piece in 'pieces " << pieceArg << "': '" << badChar << "'\n"
            "  Use P/N/B/R/Q (white) or p/n/b/r/q (black). Kings are implicit.\n";
    return;
  }

  // ---- options ------------------------------------------------------------
  const bool wantDump = utils::hasArg(args, "dump");
  const string dumpFile = wantDump ? utils::argValue(args, "dump") : string();
  const bool noFold   = utils::hasArg(args, "allfiles");
  const bool wantMirror = utils::hasArg(args, "mirror");
  const bool wantOracle = utils::hasArg(args, "oracle");
  const bool noCache  = utils::hasArg(args, "nocache");
  const int maxKingFile = noFold ? 7 : 3;

  // Oracle thread budget. `threads <n>` caps the OpenMP team used by the solver
  // (the only parallel stage) so the harness need not saturate every core. 0 =
  // not given = default to HALF the hardware threads, leaving the machine usable.
  // Clamped to [1, hardware max]; an unparseable value falls back to the default.
  int reqThreads = 0;
  if (utils::hasArg(args, "threads"))
  {
    try { reqThreads = std::stoi(utils::argValue(args, "threads")); }
    catch (...) { reqThreads = 0; }
    if (reqThreads < 1) reqThreads = 1;
  }

#ifdef _OPENMP
  const int maxThreads = omp_get_max_threads();
  if (reqThreads > maxThreads) reqThreads = maxThreads;
  // Default (no explicit request): half the cores, at least 1.
  const int usingThreads = reqThreads > 0 ? reqThreads : std::max(1, maxThreads / 2);
  omp_set_num_threads(usingThreads);
#else
  const int usingThreads = 1;
#endif

  // ---- colourings to enumerate --------------------------------------------
  vector<string> colourings{ pieceArg };
  const string mirror = flipCase(pieceArg);
  const bool selfMirror = sameMaterial(pieceArg, mirror);
  const bool haveMirror = wantMirror && !selfMirror;
  if (haveMirror)
    colourings.push_back(mirror);

  // ---- dump (shared across colourings) ------------------------------------
  // Each worker buffers its slice in memory; the merged text is written to the
  // file in task order after enumeration. Open it up front to fail fast (before
  // the long oracle solve) if the path is not writable.
  std::ofstream out;
  if (wantDump)
  {
    out.open(dumpFile);
    if (!out)
    {
      cout << "Could not open dump file: " << dumpFile << '\n';
      return;
    }
  }

  // ---- enumerate ----------------------------------------------------------
  const perf_clock start = perf::now();

  vector<Generator> gens;
  vector<std::unique_ptr<EgSolver>> solvers;   // keep oracles alive for reporting
  for (const string& cs : colourings)
  {
    Generator g;
    g.slots.push_back({'K', false}); // white king
    g.slots.push_back({'k', false}); // black king

    vector<Slot> ex;
    char b = 0;
    parseExtras(cs, ex, b);            // flip of a valid set is always valid
    for (const Slot& s : ex)
      g.slots.push_back(s);

    g.maxKingFile = maxKingFile;
    g.wantDump = wantDump;

    // Build the perfect WDL oracle for this colouring (its own capture/promotion
    // DAG), then bucket each call-set position against it inside leaf().
    if (wantOracle)
    {
      auto solver = std::make_unique<EgSolver>();
      solver->cacheEnabled = !noCache;
      vector<Piece> men;
      for (char c : cs)
        men.push_back(charToPiece(c));

      string err;
      cout << "Building oracle for " << signatureOf(cs)
           << " (" << usingThreads << " thread" << (usingThreads == 1 ? "" : "s")
           << ") ... " << std::flush;
      const perf_clock obStart = perf::now();
      if (solver->build(men, err))
      {
        const perf_time obDur = perf::now() - obStart;
        cout << "done (" << std::fixed << std::setprecision(1)
             << obDur.count() << " s";
        if (!noCache)
          cout << "; " << solver->tablesSolved << " solved, "
               << solver->tablesLoaded << " from cache";
        cout << ")";
        uint64_t w = 0, d = 0, l = 0;
        if (solver->distribution(men, w, d, l))
          cout << "  full-legal WDL: win " << w << ", draw " << d
               << ", loss " << l;
        cout << '\n';
        g.oracle = solver.get();
        solvers.push_back(std::move(solver));
      }
      else
        cout << "skipped: " << err << '\n';
    }

    g.run();
    gens.push_back(std::move(g));
  }

  if (wantDump)
  {
    for (const Generator& g : gens)
      out << g.dump;
    out.close();
  }

  const perf_time dur = perf::now() - start;

  // ---- report -------------------------------------------------------------
  cout << "\n=== Endgame validation: " << signatureOf(pieceArg) << " generator ===\n";
  cout << "(kings + " << pieceArg << "; "
       << (noFold ? "all 8 king files (self-check)"
                  : "white king folded to files a-d")
       << ", both sides to move; " << usingThreads << " thread"
       << (usingThreads == 1 ? "" : "s") << ")\n";
  if (haveMirror)
    cout << "Colourings: " << pieceArg << " (" << signatureOf(pieceArg)
         << ") and colour-mirror " << mirror
         << " (" << signatureOf(mirror) << ").\n\n";
  else if (selfMirror)
    cout << "Colouring: " << pieceArg
         << " only (colour-symmetric material; no distinct mirror).\n\n";
  else
    cout << "Colouring: " << pieceArg << " (" << signatureOf(pieceArg)
         << ") only (default; pass 'mirror' to also run the colour-mirror "
         << mirror << " (" << signatureOf(mirror)
         << ") and the colour-symmetry self-check).\n\n";

  for (size_t i = 0; i < gens.size(); ++i)
    reportColouring(gens[i], colourings[i]);

  // Colour-symmetry self-check: the two colourings are colour-swap + rank-flip
  // images, a bijection preserving legality, the call gate, and the true
  // result -- so a colour-symmetric recognizer must tally identically.
  if (haveMirror)
  {
    const bool callOk = gens[0].t.quiet    == gens[1].t.quiet;
    const bool drawOk = gens[0].t.heurDraw == gens[1].t.heurDraw;
    cout << "Colour-symmetry self-check (the two colourings must tally identically):\n";
    cout << "  call set : " << gens[0].t.quiet << " vs " << gens[1].t.quiet
         << "   " << (callOk ? "OK" : "MISMATCH") << '\n';
    cout << "  draws    : " << gens[0].t.heurDraw << " vs " << gens[1].t.heurDraw
         << "   " << (drawOk ? "OK" : "MISMATCH") << '\n';
    if (!callOk || !drawOk)
      cout << "  ** isTheoreticalDraw is colour-asymmetric -- inspect. **\n";
    cout << '\n';
  }

  // ---- oracle scorecard ---------------------------------------------------
  if (wantOracle)
  {
    for (size_t i = 0; i < gens.size(); ++i)
      if (gens[i].oracle)
        reportScorecard(gens[i], colourings[i]);

    // The two colourings are colour-swap + rank-flip images, so the safe and
    // dangerous buckets must match exactly -- a check on the oracle itself.
    if (haveMirror && gens[0].oracle && gens[1].oracle)
    {
      const bool fdOk = gens[0].t.falseDraw  == gens[1].t.falseDraw;
      const bool mdOk = gens[0].t.missedDraw == gens[1].t.missedDraw;
      cout << "Oracle colour-symmetry self-check (buckets must match):\n";
      cout << "  false-draw  : " << gens[0].t.falseDraw << " vs " << gens[1].t.falseDraw
           << "   " << (fdOk ? "OK" : "MISMATCH") << '\n';
      cout << "  missed-draw : " << gens[0].t.missedDraw << " vs " << gens[1].t.missedDraw
           << "   " << (mdOk ? "OK" : "MISMATCH") << "\n\n";
    }
  }

  if (wantOracle)
    cout << "Scorecard above is correctness: FALSE-DRAW must be 0 (the engine\n"
            "      would otherwise discard a decided game). missed-draw is a\n"
            "      safe coverage gap.\n";
  else
    cout << "NOTE: no oracle (pass 'oracle' for the WDL scorecard) -- these are\n"
            "      the heuristic's labels, not correctness.\n";
  cout << "Elapsed: " << dur.count() << " s\n";

  if (wantDump)
  {
    uint64_t dumped = 0;
    for (const Generator& g : gens)
      dumped += g.t.quiet;
    cout << "Dumped " << dumped << " positions to " << dumpFile << '\n';
  }
}
