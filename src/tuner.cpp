
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <array>

#include "tuner.h"
#include "bitboard.h"
#include "evaluation.h"
#include "base_utils.h"
#include "perf.h"

using std::cout;
using std::endl;
using std::string;
using std::vector;

namespace {

// One cached training position: the white-relative component subtotals (weight-independent)
// plus the game result label in {1.0, 0.5, 0.0}. Re-evaluating it for a candidate weight
// set is pure arithmetic via evalFromComponents — no board, movegen or attack lookups.
struct TuneEntry
{
  EvalComponents ec;
  double result;
};

// White-relative static eval. evaluate() returns it side-to-move-relative as
// score * side2move; multiplying by side2move again (it is +-1) recovers the white POV.
Score
whiteRelativeEval(const ChessBoard& pos)
{
  const Score side2move = Score(2 * int(pos.color) - 1);
  return evaluate<false>(pos) * side2move;
}

// sigmoid(K * eval / 400) using the base-10 logistic, matching the Texel error model.
double
winProbability(double eval, double K)
{
  return 1.0 / (1.0 + std::pow(10.0, -K * eval / 400.0));
}

// Mean squared error of the cached dataset under scaling constant K and weights w.
double
meanSquaredError(const vector<TuneEntry>& data, double K, const EvalWeights& w)
{
  double total = 0.0;
  for (const auto& e : data)
  {
    const Score eval = evalFromComponents(e.ec, w);
    const double diff = e.result - winProbability(eval, K);
    total += diff * diff;
  }
  return total / double(data.size());
}

// Correctness gate: the cached-component reconstruction (evalFromComponents) must
// reproduce the real white-relative eval bit-for-bit on tunable positions, and special
// endgames must be flagged non-tunable (skipped). Returns false on any mismatch so the
// caller can refuse to tune against a broken cache.
bool
runSelfCheck()
{
  // startpos, a few middlegames, low-phase / pawn endgames, and special endgames
  // (lone-king variants + bishop-pawn) that bypass the weighted eval.
  static const std::array<const char*, 12> fens = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
    "r2q1rk1/pp2bppp/2n1bn2/2pp4/3P4/2N1PN2/PP2BPPP/R1BQ1RK1 w - - 0 9",
    "r3k2r/1bp2ppp/p1np1n2/1p2p3/4P3/1BPP1N2/PP3PPP/RNBQ1RK1 b kq - 0 9",
    "8/2p2pkp/3p2p1/8/2P1P3/3P2P1/5PKP/8 w - - 0 1",
    "8/5k2/8/8/8/8/3K1P2/8 w - - 0 1",       // K+P vs K  (low phase, mg-zeroed)
    "8/8/8/4k3/8/8/4K3/8 w - - 0 1",          // K vs K    (lone king, skip)
    "8/8/8/4k3/8/8/4KQ2/8 w - - 0 1",         // KQ vs K   (lone king, skip)
    "8/8/8/4k3/8/8/3KBN2/8 w - - 0 1",        // KBN vs K  (lone king, skip)
    "8/4kp2/8/8/8/8/4KB2/8 w - - 0 1",        // KB vs KP  (bishop-pawn, skip)
    "6k1/5ppp/8/8/8/8/5PPP/6K1 w - - 0 1",
    "8/8/4k3/8/8/3K4/4P3/8 w - - 0 1"         // K+P vs K  (low phase, mg-zeroed)
  };

  int checked = 0, skipped = 0, mismatches = 0;
  Score maxDiff = 0;

  for (const char* fen : fens)
  {
    ChessBoard pos(fen);
    const EvalComponents ec = extractEvalComponents(pos);

    if (!ec.tunable) { ++skipped; continue; }

    const Score got = evalFromComponents(ec, evalWeights);
    const Score want = whiteRelativeEval(pos);
    const Score diff = Score(std::abs(int(got - want)));

    if (diff != 0)
    {
      ++mismatches;
      maxDiff = std::max(maxDiff, diff);
      cout << "  MISMATCH  got=" << got << " want=" << want
           << "  fen=" << fen << '\n';
    }
    ++checked;
  }

  cout << "Self-check: " << checked << " reconstructed, " << skipped
       << " skipped (special endgames), " << mismatches
       << " mismatches, maxDiff=" << maxDiff << '\n';

  return mismatches == 0;
}

// "1-0" -> 1.0, "0-1" -> 0.0, "1/2-1/2" -> 0.5; also tolerates numeric "1.0"/"0.5"/"0.0".
// Returns false if the token is unrecognised.
bool
parseResult(const string& token, double& out)
{
  if (token.find("1/2") != string::npos || token.find("0.5") != string::npos)
    { out = 0.5; return true; }
  if (token.find("1-0") != string::npos || token.find("1.0") != string::npos)
    { out = 1.0; return true; }
  if (token.find("0-1") != string::npos || token.find("0.0") != string::npos)
    { out = 0.0; return true; }
  return false;
}

// Pads an EPD position (4 fields: board, side, castling, ep) to a full 6-field FEN so
// the ChessBoard parser is happy; leaves already-complete FENs untouched.
string
toFullFen(const string& position)
{
  vector<string> fields;
  for (const auto& f : utils::split(position, ' '))
    if (!f.empty()) fields.push_back(f);

  if (fields.size() < 4) return position;  // malformed; let the caller skip it

  string fen = fields[0] + ' ' + fields[1] + ' ' + fields[2] + ' ' + fields[3];
  fen += (fields.size() >= 6) ? (' ' + fields[4] + ' ' + fields[5]) : string(" 0 1");
  return fen;
}

// Parses a Zurichess-style labeled EPD file (`<position> ... c9 "RESULT";` per line) and
// calls visit(pos, ec, result) for each tunable position; counts skips/parse failures.
// Returns false if the file can't be opened.
template <typename Visit>
bool
readDataset(const string& path, Visit visit)
{
  std::ifstream in(path);
  if (!in)
  {
    cout << "Could not open dataset: " << path << '\n';
    return false;
  }

  size_t lines = 0, tunable = 0, parseFail = 0, special = 0;
  string line;

  while (std::getline(in, line))
  {
    if (line.empty()) continue;
    ++lines;

    const size_t tag = line.find("c9");
    if (tag == string::npos) { ++parseFail; continue; }

    const size_t q1 = line.find('"', tag);
    const size_t q2 = (q1 == string::npos) ? string::npos : line.find('"', q1 + 1);
    if (q2 == string::npos) { ++parseFail; continue; }

    double result;
    if (!parseResult(line.substr(q1 + 1, q2 - q1 - 1), result)) { ++parseFail; continue; }

    const string fen = toFullFen(line.substr(0, tag));
    ChessBoard pos(fen);

    const EvalComponents ec = extractEvalComponents(pos);
    if (!ec.tunable) { ++special; continue; }

    visit(pos, ec, result);
    ++tunable;
  }

  cout << "Loaded " << lines << " lines: " << tunable << " tunable, "
       << special << " special-endgame skips, " << parseFail << " parse failures.\n";
  return true;
}

// Builds the component cache for every tunable position in the file.
vector<TuneEntry>
loadDataset(const string& path)
{
  vector<TuneEntry> data;
  readDataset(path, [&](const ChessBoard&, const EvalComponents& ec, double result)
    { data.push_back({ec, result}); });
  return data;
}

// Ternary search for the K that minimises mse(K) (MSE(K) is unimodal).
template <typename MseOfK>
double
fitK(MseOfK mse)
{
  double lo = 0.0, hi = 3.0;
  for (int i = 0; i < 40; ++i)
  {
    const double m1 = lo + (hi - lo) / 3.0;
    const double m2 = hi - (hi - lo) / 3.0;
    if (mse(m1) < mse(m2))
      hi = m2;
    else
      lo = m1;
  }
  return (lo + hi) / 2.0;
}

// The tunable weights, exposed as named handles into a weight set for coordinate descent.
struct WeightRef { const char* name; float EvalWeights::* member; };

static const std::array<WeightRef, 16> WEIGHTS = {{
  {"materialWeightMg",      &EvalWeights::materialWeightMg},
  {"materialWeightEg",      &EvalWeights::materialWeightEg},
  {"pieceTableWeightMg",    &EvalWeights::pieceTableWeightMg},
  {"pieceTableWeightEg",    &EvalWeights::pieceTableWeightEg},
  {"pawnStructureWeightEg", &EvalWeights::pawnStructureWeightEg},
  {"mobBishopWeightMg",     &EvalWeights::mobBishopWeightMg},
  {"mobKnightWeightMg",     &EvalWeights::mobKnightWeightMg},
  {"mobRookWeightMg",       &EvalWeights::mobRookWeightMg},
  {"mobQueenWeightMg",      &EvalWeights::mobQueenWeightMg},
  {"threatsWeightMg",       &EvalWeights::threatsWeightMg},
  {"distanceWeightEg",      &EvalWeights::distanceWeightEg},
  {"bishopPairWeightMg",    &EvalWeights::bishopPairWeightMg},
  {"bishopPairWeightEg",    &EvalWeights::bishopPairWeightEg},
  {"rookFileWeightMg",      &EvalWeights::rookFileWeightMg},
  {"isolatedPawnWeightMg",  &EvalWeights::isolatedPawnWeightMg},
  {"isolatedPawnWeightEg",  &EvalWeights::isolatedPawnWeightEg}
}};

// Coordinate descent: probe each weight +-step, keep any move that lowers MSE; when a full
// sweep yields no improvement, halve the step. Stops at a tiny step or the iteration cap.
EvalWeights
coordinateDescent(const vector<TuneEntry>& data, double K, EvalWeights best, int maxIters)
{
  // A sweep whose total MSE gain falls below this is treated as stalled at the current
  // resolution, so the step shrinks (or the search ends once the step is tiny).
  constexpr double STALL_GAIN = 1e-7;

  double bestMse = meanSquaredError(data, K, best);
  double step = 0.1;
  int iter = 0;

  while (step > 1e-3 && iter < maxIters)
  {
    const double sweepStart = bestMse;

    for (const auto& wr : WEIGHTS)
    {
      const float orig = best.*wr.member;

      best.*wr.member = orig + float(step);
      double m = meanSquaredError(data, K, best);

      if (m + 1e-12 < bestMse) { bestMse = m; continue; }

      best.*wr.member = orig - float(step);
      m = meanSquaredError(data, K, best);

      if (m + 1e-12 < bestMse) { bestMse = m; continue; }

      best.*wr.member = orig;  // neither direction helped; revert
    }

    ++iter;

    if (sweepStart - bestMse < STALL_GAIN)
    {
      step *= 0.5;  // resolution exhausted; refine
      cout << "  step " << std::fixed << std::setprecision(5) << step
           << "  mse " << std::setprecision(8) << bestMse
           << "  (" << iter << " sweeps)" << endl;
    }
  }

  return best;
}

void
printWeights(std::ostream& os, const EvalWeights& w)
{
  os << std::fixed << std::setprecision(4);
  for (const auto& wr : WEIGHTS)
    os << "  " << std::left << std::setw(24) << wr.name
       << std::right << (w.*wr.member) << '\n';
}

// Loads one dataset, fits K, runs coordinate descent, and streams the report to stdout.
// When reportPath is non-empty the same default/tuned-weight block is also written there.
// Returns false if the dataset could not be loaded (so --all can skip to the next file).
bool
tuneDataset(const string& path, int maxIters, const string& reportPath)
{
  const perf_clock start = perf::now();
  const vector<TuneEntry> data = loadDataset(path);
  if (data.empty()) { cout << "No tunable positions loaded. Skipping " << path << ".\n"; return false; }
  const perf_time buildTime = perf::now() - start;
  cout << "Cache built in " << std::fixed << std::setprecision(2)
       << buildTime.count() << " s.\n\n";

  EvalWeights start_w = evalWeights;
  const double K = fitK([&](double k) { return meanSquaredError(data, k, start_w); });
  const double mseBefore = meanSquaredError(data, K, start_w);
  cout << "Fitted K = " << std::setprecision(4) << K
       << "   MSE (defaults) = " << std::setprecision(8) << mseBefore << "\n\n";

  cout << "Coordinate descent:\n";
  const EvalWeights tuned = coordinateDescent(data, K, start_w, maxIters);
  const double mseAfter = meanSquaredError(data, K, tuned);

  // Build the result block once, then send it to stdout and (optionally) the report file.
  std::ostringstream report;
  report << "Dataset: " << path << '\n'
         << "Fitted K = " << std::fixed << std::setprecision(4) << K << '\n'
         << "MSE before = " << std::setprecision(8) << mseBefore
         << "   MSE after = " << mseAfter
         << "   (" << std::setprecision(4)
         << 100.0 * (mseBefore - mseAfter) / mseBefore << "% lower)\n\n"
         << "Default weights:\n";
  printWeights(report, start_w);
  report << "Tuned weights:\n";
  printWeights(report, tuned);

  cout << '\n' << report.str();

  if (!reportPath.empty())
  {
    std::ofstream f(reportPath);
    if (f) { f << report.str(); cout << "  -> wrote " << reportPath << '\n'; }
    else   { cout << "  ! could not write " << reportPath << '\n'; }
  }

  cout << "\nThese are NOT applied automatically — paste the values into EvalWeights "
          "defaults if arena-validated.\n";
  return true;
}

// Every *.epd in dirArg (default: the texel_dataset folder, relative to the usual output/
// working dir), sorted. Empty, with a message, if there are none.
vector<std::filesystem::path>
listDatasets(const string& dirArg)
{
  namespace fs = std::filesystem;
  const fs::path dir = dirArg.empty() ? fs::path("../Utility/texel_dataset") : fs::path(dirArg);

  vector<fs::path> datasets;
  std::error_code ec;
  if (!fs::is_directory(dir, ec))
  {
    cout << "Dataset directory not found: " << dir.string()
         << " (override with: dir <path>)\n";
    return datasets;
  }

  for (const auto& entry : fs::directory_iterator(dir))
    if (entry.is_regular_file() && entry.path().extension() == ".epd")
      datasets.push_back(entry.path());
  std::sort(datasets.begin(), datasets.end());

  if (datasets.empty()) cout << "No .epd files in " << dir.string() << '\n';
  return datasets;
}

// ---------------------------------------------------------------------------------------
// Piece-square tables: elsa tune pst [data <path> | --all [dir <folder>]] [tables <list>]
//                                    [iters <n>] [free]
//
// Coordinate descent can't move hundreds of table entries one probe at a time, so the
// tables get full-batch gradient descent (Adam) instead, with every weight and every
// other term held fixed. A position's eval is linear in the entries,
//
//   eval = fixed + phase       * pieceTableWeightMg * sum(count * mgEntry)
//                + (1 - phase) * pieceTableWeightEg * sum(count * egEntry),
//
// where the sums run over the tables being tuned and `fixed` is the rest of the eval,
// tables left out included. Each position is cached as `fixed` plus a sparse list of
// (entry, count) features. evaluate() truncates each phase's sum to a Score; the model
// skips that for a phase with a tuned table, which is under 1 cp.
// ---------------------------------------------------------------------------------------

// Each table is tuned with its files folded (a = h, b = g, c = f, d = e): 32 entries,
// rank * 4 + folded file. The parameter vector holds the six midgame tables and then the
// six endgame tables, in PieceType order.
constexpr int PST_MG      = 0;
constexpr int PST_EG      = 1;
constexpr int PST_TYPES   = KING - PAWN + 1;
constexpr int PST_TABLES  = 2 * PST_TYPES;
constexpr int PST_ENTRIES = 32;
constexpr int PST_PHASE   = PST_TYPES  * PST_ENTRIES;  // one phase's tables
constexpr int PST_PARAMS  = PST_TABLES * PST_ENTRIES;

using PstParams = array<double, PST_PARAMS>;
using PstTables = array<bool, PST_TABLES>;  // which tables are tuned

static constexpr array<const char*, PST_TABLES> PST_NAMES = {
  "pawnMg", "bishopMg", "knightMg", "rookMg", "queenMg", "kingMg",
  "pawnEg", "bishopEg", "knightEg", "rookEg", "queenEg", "kingEg"
};

constexpr int
pstTable(int phase, PieceType pt)
{ return phase * PST_TYPES + (pt - PAWN); }

constexpr PieceType
pstTableType(int table)
{ return PieceType(table % PST_TYPES + PAWN); }

// The entry's index within one phase's tables; the endgame entry is PST_PHASE further on.
constexpr int
pstParam(PieceType pt, int whiteSq)
{
  const int file = whiteSq & 7;
  return (pt - PAWN) * PST_ENTRIES + (whiteSq >> 3) * 4 + std::min(file, 7 - file);
}

struct PstFeature
{
  uint8_t param;  // pstParam(): the same entry in both phases
  int8_t  count;  // White's pieces on the entry minus Black's (on the mirrored square)
};

struct PstEntry
{
  float    fixed;    // the eval without the tuned tables
  float    mgScale;  // phase * pieceTableWeightMg
  float    egScale;  // (1 - phase) * pieceTableWeightEg
  float    result;
  uint32_t first;    // this position's features are features[first, first + size)
  uint8_t  size;
};

// One dataset's slice of PstData. `occurrences` sums the phase weight (phase for a
// midgame entry, 1 - phase for an endgame one) over every piece that stands on each
// entry; it weights the table means that centreTables() holds in place.
struct PstRange
{
  string    name;
  size_t    begin = 0, end = 0;
  PstParams occurrences = {};
};

struct PstData
{
  vector<PstEntry>   entries;
  vector<PstFeature> features;
  vector<PstRange>   ranges;
};

// `list` is comma-separated table names (queenMg, pawnEg, ...), where `mg` and `eg` stand
// for all six of that phase. Empty means `eg`. Returns false on an unknown name.
bool
parsePstTables(const string& list, PstTables& tuned)
{
  tuned = {};

  for (const string& token : utils::split(list.empty() ? "eg" : list, ','))
  {
    if (token.empty()) continue;

    bool known = false;
    for (int t = 0; t < PST_TABLES; t++)
    {
      const int phase = t / PST_TYPES;
      if (token == PST_NAMES[t] || (token == "mg" && phase == PST_MG) || (token == "eg" && phase == PST_EG))
        tuned[t] = known = true;
    }

    if (!known)
    {
      cout << "Unknown table: " << token << " (use pawnMg ... kingEg, mg or eg).\n";
      return false;
    }
  }

  return true;
}

bool
tunesPhase(const PstTables& tuned, int phase)
{
  for (int pt = PAWN; pt <= KING; pt++)
    if (tuned[pstTable(phase, PieceType(pt))]) return true;
  return false;
}

bool
appendPstDataset(PstData& data, const string& path, const string& name, const PstTables& tuned)
{
  PstRange range;
  range.name  = name;
  range.begin = data.entries.size();

  const bool tuneMg = tunesPhase(tuned, PST_MG);
  const bool tuneEg = tunesPhase(tuned, PST_EG);

  const bool ok = readDataset(path, [&](const ChessBoard& pos, const EvalComponents& ec, double result)
  {
    const float  mgWeight = ec.phase;
    const float  egWeight = 1 - ec.phase;
    const size_t first    = data.features.size();

    // The tuned tables' current share of the piece-square sums, White-relative.
    Score tunedMg = 0, tunedEg = 0;

    for (int sq = 0; sq < SQUARE_NB; sq++)
    {
      const Piece p = pos.pieceOnSquare(Square(sq));
      if (p == NO_PIECE) continue;

      const PieceType pt = type_of(p);
      const bool      mg = tuned[pstTable(PST_MG, pt)];
      const bool      eg = tuned[pstTable(PST_EG, pt)];
      if (!mg && !eg) continue;

      if (mg) tunedMg += pieceSquareTable[p][sq].mg;
      if (eg) tunedEg += pieceSquareTable[p][sq].eg;

      const bool   white = color_of(p) == WHITE;
      const int    param = pstParam(pt, white ? sq : sq ^ 56);
      const int8_t sign  = white ? 1 : -1;

      range.occurrences[param]             += mgWeight;
      range.occurrences[PST_PHASE + param] += egWeight;

      // A second piece on the same folded entry merges into the first; a white and a
      // black piece on mirrored squares cancel out.
      auto it = std::find_if(data.features.begin() + ptrdiff_t(first), data.features.end(),
                             [&](const PstFeature& f) { return f.param == param; });
      if (it != data.features.end()) it->count = int8_t(it->count + sign);
      else data.features.push_back({uint8_t(param), sign});
    }

    data.features.erase(std::remove_if(data.features.begin() + ptrdiff_t(first), data.features.end(),
                                       [](const PstFeature& f) { return f.count == 0; }),
                        data.features.end());

    // A phase with no tuned table keeps evaluate()'s truncation, so it stays exact.
    const PhaseSums sums   = phaseSumsFromComponents(ec, evalWeights);
    const float     mgRest = tuneMg ? sums.mg - evalWeights.pieceTableWeightMg * float(tunedMg)
                                    : float(Score(sums.mg));
    const float     egRest = tuneEg ? sums.eg - evalWeights.pieceTableWeightEg * float(tunedEg)
                                    : float(Score(sums.eg));

    data.entries.push_back({
      mgWeight * mgRest + egWeight * egRest,
      mgWeight * evalWeights.pieceTableWeightMg,
      egWeight * evalWeights.pieceTableWeightEg,
      float(result),
      uint32_t(first),
      uint8_t(data.features.size() - first)
    });
  });

  range.end = data.entries.size();
  if (range.end > range.begin) data.ranges.push_back(range);
  return ok;
}

// The engine's current tables, folded, with every table left out at zero (its value is
// in `fixed`). Folding changes a table that isn't left-right symmetric, so those are named.
PstParams
startingPstParams(const PstTables& tuned)
{
  PstParams theta = {}, squares = {};

  for (int t = 0; t < PST_TABLES; t++)
  {
    if (!tuned[t]) continue;

    const PieceType pt    = pstTableType(t);
    const int       base  = t / PST_TYPES * PST_PHASE;
    const auto&     row   = pieceSquareTable[make_piece(WHITE, pt)];
    bool            folds = true;

    for (int sq = 0; sq < SQUARE_NB; sq++)
    {
      const Score value = t / PST_TYPES == PST_MG ? row[sq].mg : row[sq].eg;
      const Score other = t / PST_TYPES == PST_MG ? row[sq ^ 7].mg : row[sq ^ 7].eg;

      theta  [base + pstParam(pt, sq)] += value;
      squares[base + pstParam(pt, sq)] += 1;
      folds = folds && value == other;
    }

    if (!folds)
      cout << "Note: " << PST_NAMES[t] << " isn't left-right symmetric; it starts from its folded average.\n";
  }

  for (int j = 0; j < PST_PARAMS; j++)
    if (squares[j] > 0) theta[j] /= squares[j];

  return theta;
}

double
pstEval(const PstData& data, const PstEntry& e, const PstParams& theta)
{
  double mg = 0.0, eg = 0.0;
  for (uint32_t k = e.first; k < e.first + e.size; k++)
  {
    const PstFeature& f = data.features[k];
    mg += f.count * theta[f.param];
    eg += f.count * theta[PST_PHASE + f.param];
  }

  return e.fixed + e.mgScale * mg + e.egScale * eg;
}

double
pstMse(const PstData& data, const PstRange& r, double K, const PstParams& theta)
{
  const ptrdiff_t begin = ptrdiff_t(r.begin), end = ptrdiff_t(r.end);
  double total = 0.0;

  #pragma omp parallel for schedule(static) reduction(+:total)
  for (ptrdiff_t i = begin; i < end; i++)
  {
    const PstEntry& e = data.entries[size_t(i)];
    const double diff = e.result - winProbability(pstEval(data, e, theta), K);
    total += diff * diff;
  }

  return total / double(end - begin);
}

// Each table's occurrence-weighted mean: how much it adds to the piece's value on average
// over the data, in table units (before pieceTableWeight).
array<double, PST_TABLES>
tableMeans(const PstParams& theta, const PstParams& occurrences)
{
  array<double, PST_TABLES> means = {};

  for (int t = 0; t < PST_TABLES; t++)
  {
    double sum = 0.0, weight = 0.0;
    for (int j = t * PST_ENTRIES; j < (t + 1) * PST_ENTRIES; j++)
    {
      sum    += theta[j] * occurrences[j];
      weight += occurrences[j];
    }
    means[t] = weight > 0.0 ? sum / weight : 0.0;
  }

  return means;
}

// Both helpers below touch only the tuned tables, and never the king's: every position
// has one king a side, so a shift of a king table cancels and its mean has no gradient.

// Removes the part of each table's gradient along its occurrence vector, the direction
// that would change what the piece is worth on average rather than where it stands.
void
removeMeanDirection(PstParams& grad, const PstParams& occurrences, const PstTables& tuned)
{
  for (int t = 0; t < PST_TABLES; t++)
  {
    if (!tuned[t] || pstTableType(t) == KING) continue;

    const int base = t * PST_ENTRIES;
    double dot = 0.0, norm = 0.0;

    for (int j = base; j < base + PST_ENTRIES; j++)
    {
      dot  += grad[j] * occurrences[j];
      norm += occurrences[j] * occurrences[j];
    }

    if (norm > 0.0)
      for (int j = base; j < base + PST_ENTRIES; j++)
        grad[j] -= dot / norm * occurrences[j];
  }
}

// Shifts each table back to its target mean. Entries no piece stands on (the pawn's
// first and last ranks) stay where they are.
void
centreTables(PstParams& theta, const PstParams& occurrences, const PstTables& tuned,
             const array<double, PST_TABLES>& targets)
{
  const array<double, PST_TABLES> means = tableMeans(theta, occurrences);

  for (int t = 0; t < PST_TABLES; t++)
  {
    if (!tuned[t] || pstTableType(t) == KING) continue;

    for (int j = t * PST_ENTRIES; j < (t + 1) * PST_ENTRIES; j++)
      if (occurrences[j] > 0.0) theta[j] -= means[t] - targets[t];
  }
}

// Full-batch Adam over one range of the data, moving only the tuned tables. With
// `centre`, every tuned table but the king's keeps the mean it started with (zero for a
// table that starts empty): the tables move pieces around without changing what they
// are worth on average, so material stays the anchor.
PstParams
adamPst(const PstData& data, const PstRange& r, double K, PstParams theta, int epochs,
        bool centre, const PstTables& tuned)
{
  constexpr double BETA1 = 0.9, BETA2 = 0.999, EPSILON = 1e-8;
  constexpr double LR_START = 1.0, LR_END = 0.1;  // table units per epoch

  const ptrdiff_t begin   = ptrdiff_t(r.begin), end = ptrdiff_t(r.end);
  const auto      targets = tableMeans(theta, r.occurrences);
  PstParams m = {}, v = {};

  for (int t = 1; t <= epochs; t++)
  {
    PstParams grad = {};
    double loss = 0.0;

    #pragma omp parallel
    {
      PstParams g = {};
      double l = 0.0;

      #pragma omp for schedule(static) nowait
      for (ptrdiff_t i = begin; i < end; i++)
      {
        const PstEntry& e = data.entries[size_t(i)];
        const double p    = winProbability(pstEval(data, e, theta), K);
        const double diff = p - e.result;
        l += diff * diff;

        // d(diff^2) / d(entry), up to a constant factor that Adam normalises away.
        const double scale = diff * p * (1.0 - p);
        for (uint32_t k = e.first; k < e.first + e.size; k++)
        {
          const PstFeature& f = data.features[k];
          g[f.param]             += scale * e.mgScale * f.count;
          g[PST_PHASE + f.param] += scale * e.egScale * f.count;
        }
      }

      #pragma omp critical
      {
        for (int j = 0; j < PST_PARAMS; j++) grad[j] += g[j];
        loss += l;
      }
    }

    for (int j = 0; j < PST_PARAMS; j++)
      if (!tuned[j / PST_ENTRIES]) grad[j] = 0.0;

    if (centre) removeMeanDirection(grad, r.occurrences, tuned);

    const double lr = LR_START + (LR_END - LR_START) * double(t - 1) / double(epochs);
    for (int j = 0; j < PST_PARAMS; j++)
    {
      m[j] = BETA1 * m[j] + (1 - BETA1) * grad[j];
      v[j] = BETA2 * v[j] + (1 - BETA2) * grad[j] * grad[j];

      const double mHat = m[j] / (1 - std::pow(BETA1, t));
      const double vHat = v[j] / (1 - std::pow(BETA2, t));
      theta[j] -= lr * mHat / (std::sqrt(vHat) + EPSILON);
    }

    if (centre) centreTables(theta, r.occurrences, tuned, targets);

    if (t == 1 || t % 100 == 0 || t == epochs)
      cout << "  epoch " << std::setw(5) << t << "  mse " << std::fixed << std::setprecision(8)
           << loss / double(end - begin) << endl;
  }

  return theta;
}

// The tuned tables as C++, ready to paste into PieceSquareTable.cpp (rank 1 first) and in
// that file's order. Each is followed by its occurrence-weighted mean (see tableMeans())
// and by how far it moved from where it started, as the RMS change over the entries
// pieces stand on.
void
printPstTables(std::ostream& os, const PstParams& theta, const PstParams& start,
               const PstParams& occurrences, const PstTables& tuned)
{
  static constexpr array<PieceType, PST_TYPES> FILE_ORDER = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};

  const array<double, PST_TABLES> means = tableMeans(theta, occurrences);

  for (int phase : {PST_MG, PST_EG})
    for (PieceType pt : FILE_ORDER)
    {
      const int t = pstTable(phase, pt);
      if (!tuned[t]) continue;

      double moved = 0.0;
      int    used  = 0;
      for (int j = t * PST_ENTRIES; j < (t + 1) * PST_ENTRIES; j++)
        if (occurrences[j] > 0.0)
        {
          moved += (theta[j] - start[j]) * (theta[j] - start[j]);
          used  += 1;
        }

      os << "static constexpr ScoreTable " << PST_NAMES[t] << " = {\n";
      for (int rank = 0; rank < 8; rank++)
      {
        os << ' ';
        for (int file = 0; file < 8; file++)
          os << std::setw(5) << std::lround(theta[phase * PST_PHASE + pstParam(pt, rank * 8 + file)]) << ',';
        os << '\n';
      }
      os << "};  // weighted mean " << std::fixed << std::setprecision(2) << means[t]
         << ", moved " << (used > 0 ? std::sqrt(moved / used) : 0.0) << " (rms)\n\n";
    }
}

// Loads the dataset(s), then tunes the chosen tables on each one and, when there are
// several, on all of them together. Each result also goes to tune_pst_<name>.txt, or
// tune_pst_<tables>_<name>.txt when `tables` is given.
void
tunePieceSquare(const vector<string>& args, int epochs)
{
  const string tablesArg = utils::argValue(args, "tables");
  PstTables tuned;
  if (!parsePstTables(tablesArg, tuned)) return;

  vector<std::pair<string, string>> files;  // name, path

  if (utils::hasArg(args, "--all") || utils::hasArg(args, "all"))
  {
    for (const auto& p : listDatasets(utils::argValue(args, "dir")))
      files.emplace_back(p.stem().string(), p.string());
  }
  else if (!utils::argValue(args, "data").empty())
  {
    const string path = utils::argValue(args, "data");
    files.emplace_back(std::filesystem::path(path).stem().string(), path);
  }

  if (files.empty())
  {
    cout << "No dataset given (use: elsa tune pst data <path.epd>, or elsa tune pst --all).\n";
    return;
  }

  string tableList;
  for (int t = 0; t < PST_TABLES; t++)
    if (tuned[t]) tableList += (tableList.empty() ? "" : " ") + string(PST_NAMES[t]);
  cout << "Tuning " << tableList << '\n';

  const perf_clock loadStart = perf::now();
  PstData data;
  for (const auto& [name, path] : files)
  {
    cout << "Loading " << path << '\n';
    appendPstDataset(data, path, name, tuned);
  }
  if (data.ranges.empty()) { cout << "No tunable positions loaded.\n"; return; }

  if (data.ranges.size() > 1)
  {
    PstRange all;
    all.name = "combined";
    all.end  = data.entries.size();
    for (const PstRange& r : data.ranges)
      for (int j = 0; j < PST_PARAMS; j++) all.occurrences[j] += r.occurrences[j];
    data.ranges.push_back(all);
  }

  const perf_time loadTime = perf::now() - loadStart;
  cout << "Cached " << data.entries.size() << " positions, " << data.features.size()
       << " features in " << std::fixed << std::setprecision(1) << loadTime.count() << " s.\n";

  const bool      centre = !utils::hasArg(args, "free");
  const PstParams start  = startingPstParams(tuned);

  string prefix = "tune_pst_";
  if (!tablesArg.empty())
  {
    string tag = tablesArg;
    std::replace(tag.begin(), tag.end(), ',', '+');
    prefix += tag + '_';
  }

  for (const PstRange& r : data.ranges)
  {
    cout << "\n================ " << r.name << " (" << (r.end - r.begin)
         << " positions) ================\n";

    const perf_clock tuneStart = perf::now();
    const double K = fitK([&](double k) { return pstMse(data, r, k, start); });
    const double mseBefore = pstMse(data, r, K, start);

    cout << "Fitted K = " << std::setprecision(4) << K << "   MSE (current tables) = "
         << std::setprecision(8) << mseBefore << "\n\n";

    const PstParams fitted   = adamPst(data, r, K, start, epochs, centre, tuned);
    const double    mseAfter = pstMse(data, r, K, fitted);
    const perf_time tuneTime = perf::now() - tuneStart;

    std::ostringstream report;
    report << "Dataset: " << r.name << " (" << (r.end - r.begin) << " positions), "
           << epochs << " epochs, " << (centre ? "centred" : "free") << '\n'
           << "Tables: " << tableList << '\n'
           << "Fitted K = " << std::fixed << std::setprecision(4) << K << '\n'
           << "MSE before = " << std::setprecision(8) << mseBefore
           << "   MSE after = " << mseAfter
           << "   (" << std::setprecision(4)
           << 100.0 * (mseBefore - mseAfter) / mseBefore << "% lower)\n\n";
    printPstTables(report, fitted, start, r.occurrences, tuned);

    cout << '\n' << report.str() << "Tuned in " << std::setprecision(1)
         << tuneTime.count() << " s.\n";

    const string reportPath = prefix + r.name + ".txt";
    std::ofstream f(reportPath);
    if (f) { f << report.str(); cout << "  -> wrote " << reportPath << '\n'; }
    else   { cout << "  ! could not write " << reportPath << '\n'; }
  }
}

}  // namespace

void
tuneEval(const vector<string>& args)
{
  cout << "\n--- Texel weight tuner ---\n";

  if (!runSelfCheck())
  {
    cout << "Self-check FAILED: component reconstruction does not match evaluate(). "
            "Aborting before tuning.\n";
    return;
  }

  // pst: gradient-tune piece-square tables instead of the weights.
  if (utils::hasArg(args, "pst"))
  {
    tunePieceSquare(args, utils::hasArg(args, "iters")
                        ? std::stoi(utils::argValue(args, "iters")) : 2000);
    return;
  }

  const int maxIters = utils::hasArg(args, "iters")
                     ? std::stoi(utils::argValue(args, "iters")) : 10000;

  // --all: tune every *.epd in the dataset directory in turn, writing each result to
  // its own tune_<stem>.txt alongside the stdout report. Directory defaults to the
  // texel_dataset folder (relative to the usual output/ working dir); override with
  // `dir <path>`.
  if (utils::hasArg(args, "--all") || utils::hasArg(args, "all"))
  {
    const vector<std::filesystem::path> datasets = listDatasets(utils::argValue(args, "dir"));
    if (datasets.empty()) return;

    cout << "Tuning across " << datasets.size() << " dataset(s):\n";
    for (const auto& p : datasets) cout << "  - " << p.filename().string() << '\n';

    size_t idx = 0;
    for (const auto& p : datasets)
    {
      ++idx;
      cout << "\n================ [" << idx << '/' << datasets.size() << "] "
           << p.filename().string() << " ================\n";
      const string reportPath = "tune_" + p.stem().string() + ".txt";
      tuneDataset(p.string(), maxIters, reportPath);
    }
    cout << "\nAll datasets done.\n";
    return;
  }

  const string dataPath = utils::argValue(args, "data");
  if (dataPath.empty())
  {
    cout << "No dataset given (use: elsa tune data <path.epd>, or elsa tune --all). "
            "Self-check only.\n";
    return;
  }

  tuneDataset(dataPath, maxIters, "");
}
