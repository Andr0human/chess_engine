
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <array>
#include <vector>
#include <algorithm>

#include "endgame_validation.h"
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
struct Generator
{
  vector<Slot> slots;          // index 0 = white king, 1 = black king, then extras
  static constexpr int WK = 0;
  static constexpr int BK = 1;

  int maxKingFile = 3;         // 3 = folded (a-d); 7 = allfiles self-check
  std::ofstream* out = nullptr;

  Color stm = WHITE;
  std::array<int, 16> square{};   // current square per slot
  std::array<int, 16> prevSame{}; // nearest earlier slot with same fenChar, or -1

  // Counters.
  uint64_t geom = 0, rejInCheck = 0;
  uint64_t legal = 0, legalW = 0, legalB = 0;
  uint64_t rejTerminal = 0, rejCaptures = 0;
  uint64_t quiet = 0, quietW = 0, quietB = 0;
  uint64_t heurDraw = 0, heurNonDraw = 0, heurDrawW = 0, heurDrawB = 0;

  string
  buildFen() const
  {
    char sq[64] = {0};
    for (int i = 0; i < static_cast<int>(slots.size()); ++i)
      sq[square[i]] = slots[i].fenChar;

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
    ++geom;
    const string fen = buildFen();
    ChessBoard pos(fen);

    if (sideNotToMoveInCheck(pos, stm))
    { ++rejInCheck; return; }

    ++legal;
    (stm == WHITE ? legalW : legalB)++;

    // Match the engine's gate exactly: isTheoreticalDraw is consulted only on
    // non-terminal positions with no captures available for the side to move
    // (single_thread.cpp:51, :331). Positions the search would never hand to the
    // recognizer must not pollute the tally.
    const MoveList moves = generateMoves(pos);
    if (!moves.anyMove())
    { ++rejTerminal; return; }            // checkmate / stalemate
    if (moves.exists<MType::CAPTURES>(pos))
    { ++rejCaptures; return; }            // a capture is available

    ++quiet;
    (stm == WHITE ? quietW : quietB)++;

    const bool isDraw = isTheoreticalDraw(pos);
    if (isDraw)
    {
      ++heurDraw;
      (stm == WHITE ? heurDrawW : heurDrawB)++;
    }
    else
      ++heurNonDraw;

    if (out)
      (*out) << fen << " | " << (isDraw ? 'D' : '.') << '\n';
  }

  // Place the man for `slot`, then recurse. Cheap geometric rejects (file fold,
  // pawn rank, overlap, king adjacency) prune whole subtrees before the
  // expensive FEN-build + parse + in-check test at the leaf.
  void
  place(int slot)
  {
    if (slot == static_cast<int>(slots.size()))
    { leaf(); return; }

    const Slot& sp = slots[slot];

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
      if (prevSame[slot] >= 0 && sq <= square[prevSame[slot]])
        continue;

      // Kings never adjacent (checked the moment the black king lands).
      if (slot == BK && kingDistance(square[WK], sq) <= 1)
        continue;

      square[slot] = sq;
      place(slot + 1);
    }
  }

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

  void run(Color s) { stm = s; computePrevSame(); place(0); }
};

// Print one colouring's tally block (no header / footer).
void
reportColouring(const Generator& g, const string& pieceStr)
{
  cout << "--- Colouring " << signatureOf(pieceStr)
       << " (pieces " << pieceStr << ") ---\n";

  cout << "Geometric placements    : " << g.geom
       << "  (distinct sq, kings >= 2 apart, pawns on 2-7)\n";
  cout << "  rejected (in check)   : " << g.rejInCheck
       << "  (side not to move)\n";
  cout << "Legal positions         : " << g.legal
       << "  (stm White " << g.legalW
       << ", stm Black " << g.legalB << ")\n";
  cout << "  rejected (terminal)   : " << g.rejTerminal
       << "  (checkmate / stalemate -- no moves)\n";
  cout << "  rejected (has capture): " << g.rejCaptures
       << "  (search skips the recognizer here)\n";
  cout << "Recognizer call set     : " << g.quiet
       << "  (stm White " << g.quietW
       << ", stm Black " << g.quietB << ")\n";

  const auto pct = [&] (uint64_t n) {
    return g.quiet
      ? (100.0 * static_cast<double>(n) / static_cast<double>(g.quiet))
      : 0.0;
  };

  cout << std::fixed << std::setprecision(2);
  cout << "  isTheoreticalDraw true  : " << g.heurDraw
       << "  (" << pct(g.heurDraw) << "% of call set)"
       << "  [stm W " << g.heurDrawW
       << ", B " << g.heurDrawB << "]\n";
  cout << "  isTheoreticalDraw false : " << g.heurNonDraw
       << "  (" << pct(g.heurNonDraw) << "% of call set)\n\n";
}

} // namespace

void
validateEndgame(const vector<string>& args)
{
  // elsa egvalidate [pieces <set>] [dump <file>] [allfiles] [onecolor]
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
  // By default BOTH colourings of the material are enumerated: the set you name
  // and its colour-mirror (e.g. Pb and pB), related by colour-swap + rank-flip.
  // A correct (colour-symmetric) recognizer must give identical call-set and
  // draw counts for the two -- a built-in self-check. `onecolor` restricts to
  // the literal set; a self-mirror material (e.g. Pp) has only one colouring.
  //
  // `allfiles` disables the white-king fold (each tally must then double).

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
  const bool oneColor = utils::hasArg(args, "onecolor");
  const int maxKingFile = noFold ? 7 : 3;

  // ---- colourings to enumerate --------------------------------------------
  vector<string> colourings{ pieceArg };
  const string mirror = flipCase(pieceArg);
  const bool selfMirror = sameMaterial(pieceArg, mirror);
  const bool haveMirror = !oneColor && !selfMirror;
  if (haveMirror)
    colourings.push_back(mirror);

  // ---- dump (shared across colourings) ------------------------------------
  std::ofstream out;
  std::ofstream* outPtr = nullptr;
  if (wantDump)
  {
    out.open(dumpFile);
    if (!out)
    {
      cout << "Could not open dump file: " << dumpFile << '\n';
      return;
    }
    outPtr = &out;
  }

  // ---- enumerate ----------------------------------------------------------
  const perf_clock start = perf::now();

  vector<Generator> gens;
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
    g.out = outPtr;
    g.run(WHITE);
    g.run(BLACK);
    gens.push_back(std::move(g));
  }

  if (wantDump)
    out.close();

  const perf_time dur = perf::now() - start;

  // ---- report -------------------------------------------------------------
  cout << "\n=== Endgame validation: " << signatureOf(pieceArg) << " generator ===\n";
  cout << "(kings + " << pieceArg << "; "
       << (noFold ? "all 8 king files (self-check)"
                  : "white king folded to files a-d")
       << ", both sides to move)\n";
  if (haveMirror)
    cout << "Colourings: " << pieceArg << " (" << signatureOf(pieceArg)
         << ") and colour-mirror " << mirror
         << " (" << signatureOf(mirror) << ").\n\n";
  else if (oneColor && !selfMirror)
    cout << "Colourings: " << pieceArg << " only (onecolor; mirror "
         << mirror << " suppressed).\n\n";
  else
    cout << "Colouring: " << pieceArg
         << " only (colour-symmetric material; no distinct mirror).\n\n";

  for (size_t i = 0; i < gens.size(); ++i)
    reportColouring(gens[i], colourings[i]);

  // Colour-symmetry self-check: the two colourings are colour-swap + rank-flip
  // images, a bijection preserving legality, the call gate, and the true
  // result -- so a colour-symmetric recognizer must tally identically.
  if (haveMirror)
  {
    const bool callOk = gens[0].quiet    == gens[1].quiet;
    const bool drawOk = gens[0].heurDraw == gens[1].heurDraw;
    cout << "Colour-symmetry self-check (the two colourings must tally identically):\n";
    cout << "  call set : " << gens[0].quiet << " vs " << gens[1].quiet
         << "   " << (callOk ? "OK" : "MISMATCH") << '\n';
    cout << "  draws    : " << gens[0].heurDraw << " vs " << gens[1].heurDraw
         << "   " << (drawOk ? "OK" : "MISMATCH") << '\n';
    if (!callOk || !drawOk)
      cout << "  ** isTheoreticalDraw is colour-asymmetric -- inspect. **\n";
    cout << '\n';
  }

  cout << "NOTE: no oracle yet -- these are the heuristic's labels, not\n"
          "      correctness. Win/draw ground truth is the next stage.\n";
  cout << "Elapsed: " << dur.count() << " s\n";

  if (wantDump)
  {
    uint64_t dumped = 0;
    for (const Generator& g : gens)
      dumped += g.quiet;
    cout << "Dumped " << dumped << " positions to " << dumpFile << '\n';
  }
}
