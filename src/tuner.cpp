
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
winProbability(Score eval, double K)
{
  return 1.0 / (1.0 + std::pow(10.0, -K * double(eval) / 400.0));
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

// Parses a Zurichess-style labeled EPD line: `<position> ... c9 "RESULT";`.
// Builds the component cache for tunable positions; counts skips/parse failures.
vector<TuneEntry>
loadDataset(const string& path)
{
  vector<TuneEntry> data;
  std::ifstream in(path);
  if (!in)
  {
    cout << "Could not open dataset: " << path << '\n';
    return data;
  }

  size_t lines = 0, parseFail = 0, special = 0;
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

    data.push_back({ec, result});
  }

  cout << "Loaded " << lines << " lines: " << data.size() << " tunable, "
       << special << " special-endgame skips, " << parseFail << " parse failures.\n";
  return data;
}

// Ternary search for the K that minimises MSE at the current weights (MSE(K) is unimodal).
double
fitK(const vector<TuneEntry>& data, const EvalWeights& w)
{
  double lo = 0.0, hi = 3.0;
  for (int i = 0; i < 40; ++i)
  {
    const double m1 = lo + (hi - lo) / 3.0;
    const double m2 = hi - (hi - lo) / 3.0;
    if (meanSquaredError(data, m1, w) < meanSquaredError(data, m2, w))
      hi = m2;
    else
      lo = m1;
  }
  return (lo + hi) / 2.0;
}

// The 12 tunable weights, exposed as named handles into a weight set for coordinate descent.
struct WeightRef { const char* name; float EvalWeights::* member; };

static const std::array<WeightRef, 12> WEIGHTS = {{
  {"materialWeightMg",      &EvalWeights::materialWeightMg},
  {"materialWeightEg",      &EvalWeights::materialWeightEg},
  {"pieceTableWeightMg",    &EvalWeights::pieceTableWeightMg},
  {"pieceTableWeightEg",    &EvalWeights::pieceTableWeightEg},
  {"pawnStructureWeightMg", &EvalWeights::pawnStructureWeightMg},
  {"pawnStructureWeightEg", &EvalWeights::pawnStructureWeightEg},
  {"mobBishopWeightMg",     &EvalWeights::mobBishopWeightMg},
  {"mobKnightWeightMg",     &EvalWeights::mobKnightWeightMg},
  {"mobRookWeightMg",       &EvalWeights::mobRookWeightMg},
  {"mobQueenWeightMg",      &EvalWeights::mobQueenWeightMg},
  {"threatsWeightMg",       &EvalWeights::threatsWeightMg},
  {"distanceWeightEg",      &EvalWeights::distanceWeightEg}
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
  const double K = fitK(data, start_w);
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

  const int maxIters = utils::hasArg(args, "iters")
                     ? std::stoi(utils::argValue(args, "iters")) : 10000;

  // --all: tune every *.epd in the dataset directory in turn, writing each result to
  // its own tune_<stem>.txt alongside the stdout report. Directory defaults to the
  // texel_dataset folder (relative to the usual output/ working dir); override with
  // `dir <path>`.
  if (utils::hasArg(args, "--all") || utils::hasArg(args, "all"))
  {
    namespace fs = std::filesystem;
    const string dirArg = utils::argValue(args, "dir");
    const fs::path dir = dirArg.empty() ? fs::path("../Utility/texel_dataset") : fs::path(dirArg);

    std::error_code ec;
    if (!fs::is_directory(dir, ec))
    {
      cout << "Dataset directory not found: " << dir.string()
           << " (override with: elsa tune --all dir <path>)\n";
      return;
    }

    vector<fs::path> datasets;
    for (const auto& entry : fs::directory_iterator(dir))
      if (entry.is_regular_file() && entry.path().extension() == ".epd")
        datasets.push_back(entry.path());
    std::sort(datasets.begin(), datasets.end());

    if (datasets.empty()) { cout << "No .epd files in " << dir.string() << '\n'; return; }

    cout << "Tuning across " << datasets.size() << " dataset(s) in " << dir.string() << ":\n";
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
