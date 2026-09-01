
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cmath>
#include "task.h"
#include "search.h"
#include "single_thread.h"
#include "test_positions.h"
#include "tuner.h"
#include "endgame.h"
#include "endgame_validation.h"
#include "perpetual.h"

void
init(const vector<string>& args)
{
  perf_clock start = perf::now();
  plt::init();

  // Zobrist keys must always be seeded — hashValue (and therefore repetition
  // detection) depends on them even when the TT is disabled. Allocating the
  // table itself stays gated behind USE_TT (resize() re-seeds, harmlessly).
  tt.getRandomKeys();

  if constexpr (USE_TT) {
    tt.resize(0);
  }
  perf_time dur = perf::now() - start;
  const auto it = dur.count();

  bool sec = (it >= 1);

  if (utils::hasArg(args, "debug"))
  {
    cout << "Table Gen. took " << (sec ? it : it * 1000)
         << (sec ? " s.\n" : " ms.") << endl;

    if constexpr (USE_TT) {
      cout << "Transposition Table Size = " << tt.size()
          << "\n\n" << std::flush;
    }
  }
}

static vector<TestPosition>
getTestPositions(const std::vector<std::string>& suite, const std::string& testType)
{
  vector<TestPosition> testPositions;

  for (const auto& testData : suite)
    testPositions.emplace_back(TestPosition(testData, testType));

  return testPositions;
}

static void
accuracyTest()
{
  // Argument : elsa accuracy

  const auto runTests = [] (const vector<TestPosition>& positions)
  {
    int posNo = 1;
    uint64_t totalNs = 0;

    for (const auto& pos : positions)
    {
      const auto depth = pos.maxDepth();

      const auto start = perf::now();
      const bool passed = pos.test(bulkCount, depth);
      const auto ns = std::chrono::duration_cast<perf_ns_time>(
        perf::now() - start
      ).count();

      totalNs += static_cast<uint64_t>(ns);

      if (!passed)
        return std::make_pair(false, totalNs);

      cout << "Position " << (posNo++) << " passed!"
           << "  (" << std::fixed << std::setprecision(1)
           << static_cast<double>(ns) / 1e6 << " ms)" << std::endl;
    }

    const double avgMs = static_cast<double>(totalNs)
                       / (static_cast<double>(positions.size()) * 1e6);
    cout << "\nOverall accuracy timing:\n"
         << "  Total   : " << std::fixed << std::setprecision(1)
         << static_cast<double>(totalNs) / 1e6 << " ms\n"
         << "  Avg/pos : " << avgMs << " ms\n";

    return std::make_pair(true, totalNs);
  };

  const auto failedTests = [] (const vector<TestPosition>& positions)
  {
    TestPosition bestCase = positions.front();
    Depth minDepth = 100;

    for (const auto& pos : positions)
    {
      Depth depth = pos.maxDepth();
      for (Depth dep = 1; dep <= depth; dep++)
      {
        if (pos.test(bulkCount, dep))
          continue;

        if (dep < minDepth)
        {
          bestCase = pos, minDepth = dep;
          break;
        }
      }
    }
    return bestCase;
  };

  const auto positions = getTestPositions(test_data::accuracy::suite1, "accuracy");
  const auto [testSuccess, _] = runTests(positions);

  if (testSuccess) return;

  cout << "Accuracy Test Failed!!\n" 
          "Looking for best_case:\n\n" << std::flush;

  const auto bestCase = failedTests(positions);

  cout << "Best Failed Case : \n";
  cout << "Fen = " << bestCase.getFen() << endl;
}

static void
helper()
{
  // Argument : elsa help

  puts("/****************   Command List   ****************/\n");

  puts("** For Elsa's movegenerator self-accuracy test, type:\n");
  puts("** elsa accuracy\n");
  
  puts("** For Elsa's movegenerator self-speed test, type:\n");
  puts("** elsa speed\n");

  puts("** For Bulk-Counting, type:\n");
  puts("** elsa count [fen <fen>] [depth <depth>]\n");

  puts("** For Evaluating a position, type:\n");
  puts("** elsa go [fen <fen>] [time <search_time>] [depth <depth>] [debug]\n");

  puts("** For debugging movegenerator, type:\n");
  puts("** elsa debug [fen <fen>] [depth <depth>] [output <filename>]\n");
  
  puts("** For static evaluation of a position, type:\n");
  puts("** elsa static [fen <fen>]\n");

  puts("** To query the theoretical-draw recognizer, type:\n");
  puts("** elsa isDraw [fen <fen>]\n");

  puts("** To validate the draw recognizer over a material signature, type:\n");
  puts("** elsa egvalidate [pieces <set>] [oracle] [threads <n>] [mirror] [nocache] [allfiles] [dump <file>]\n");
  puts("**                  [nocapgate] [combos] [sums] [frozen] [freeze <n>] [maxk <n>] [top <n>]\n");
  puts("**   e.g. 'elsa egvalidate pieces Pb oracle threads 4'  (KPKB vs perfect WDL, 4 threads)\n");
  puts("**        add 'mirror' to also run the colour-mirror (KBKP) colour-symmetry self-check\n");
  puts("**        'nocapgate' keeps has-capture positions in the call set (diagnostic: search never asks there)\n");
  puts("**        oracle tables cache under output/egcache/ (sub-second reload); 'nocache' forces a fresh solve\n");
  puts("**        bucket-probe searches (need 'oracle'): combos=feature subsets, sums=signed inequalities,\n");
  puts("**        frozen=inequalities as coordinates; maxk/top bound them, freeze <n> sets frozen slots\n");

  puts("** For tuning evaluation weights (Texel), type:\n");
  puts("** elsa tune [data <path.epd>] [iters <n>]\n");
  puts("** elsa tune --all [dir <folder>] [iters <n>]   (tune every .epd in folder)\n");

  puts("** Note: Commands and flags can be in any order\n");
  puts("         (e.g. 'elsa debug depth 3 fen <fen>' or 'elsa fen <fen> go')\n");

  puts("/**************************************************/\n\n");
}

static void
speedTest()
{
  // Argument : elsa speed

  const auto positions = getTestPositions(test_data::speed::suite1, "speed");
  cout << "Positions Found : " << positions.size() << '\n';
  
  int64_t totalTime = 0;
  Nodes totalNodes = 0;
  const int loop = 3;
  int positionNo = 1;

  for (auto pos : positions)
  {
    Nodes currentNodes = pos.nodeCount(1) * loop;
    uint64_t currentTime = pos.time(bulkCount, loop);

    totalNodes += currentNodes;
    totalTime  += currentTime;

    const uint64_t currentSpeed = currentNodes / currentTime;
    cout << "position-" << positionNo++ << "\t: " << currentSpeed << " M nodes/sec." << endl;
  }

  const uint64_t speed = totalNodes / totalTime;
  cout << "Single Thread Speed : " << speed << " M nodes/sec." << endl;
}

static void
setParamswithDifficulty(string difficulty, double& searchTime, Depth& searchDepth)
{
  if (difficulty == "beginner") {
    searchTime = 0.001;
    searchDepth = 1;
  } else if (difficulty == "easy") {
    searchTime = 0.005;
    searchDepth = 3;
  } else if (difficulty == "medium") {
    searchTime = 0.1;
    searchDepth = 5;
  } else if (difficulty == "hard") {
    searchTime = 1;
    searchDepth = 8;
  } else if (difficulty == "expert") {
    searchTime = 1.5;
    searchDepth = MAX_DEPTH;
  }
}

static CheckOrder
checkOrderFromName(const string& name, CheckOrder fallback)
{
  return name == "none"      ? CheckOrder::NONE
       : name == "near"      ? CheckOrder::NEAR
       : name == "far"       ? CheckOrder::FAR
       : name == "heavy"     ? CheckOrder::HEAVY
       : name == "heavynear" ? CheckOrder::HEAVY_NEAR
       :                       fallback;
}

static void
directSearch(const vector<string> &args)
{
  // elsa [depth <depth>] [fen <fen>] [debug] [time <search_time>] go

  const string fen = utils::getFen(args, START_FEN);
  const double searchTime = utils::getTime(args, DEFAULT_SEARCH_TIME);
  const Depth searchDepth = utils::getDepth(args, MAX_DEPTH);

  ChessBoard pos(fen);
  search(pos, searchDepth, searchTime, std::cout, true);
}

static void
bestMoveSearch(const vector<string> &args)
{
  // elsa bestmove [fen <fen>] [difficulty <difficulty>] [depth <depth>] [time <search_time>]

  const string fen = utils::getFen(args, START_FEN);
  const string difficulty = utils::getDifficulty(args, "expert");
  Depth depth = utils::getDepth(args, MAX_DEPTH);
  double searchTime = utils::getTime(args, DEFAULT_SEARCH_TIME);

  setParamswithDifficulty(difficulty, searchTime, depth);

  ChessBoard pos(fen);
  search(pos, depth, searchTime, std::cout, false);
  const auto bestMove = info.lastIterationResult();
  cout << printMove(bestMove.first, pos) << endl;

  pos.makeMove(bestMove.first);
  cout << pos.fen() << endl;
}

static void
nodeCount(const vector<string> &args)
{
  // elsa [depth <depth>] [fen <fen>] count

  const string fen = utils::getFen(args, START_FEN);
  const Depth depth = utils::getDepth(args, 6);

  ChessBoard pos(fen);

  cout << "Fen = " << fen << '\n';
  cout << "Depth = " << depth << "\n" << endl;

  const auto &[nodes, t] = perf::run_algo(bulkCount, pos, depth);

  cout << "Nodes(single-thread) = " << nodes << '\n';
  cout << "Time (single-thread) = " << t << " sec.\n";
  cout << "Speed(single-thread) = " << static_cast<double>(nodes) / (t * 1e6)
        << " M nodes/sec.\n" << endl;
}

static void
debugMoveGenerator(const vector<string> &args)
{
  // elsa [fen <fen>] [depth <depth>] [output <filename>] [movegen]

  const auto moveName = [] (int move)
  {
    int ip = (move & 63);
    int fp = (move >> 6) & 63;

    int ix = ip & 7, iy = (ip - ix) >> 3;
    int fx = fp & 7, fy = (fp - fx) >> 3;

    char a1 = static_cast<char>(97 + ix);             // 'a' + ix
    char a2 = static_cast<char>(49 + iy);             // '1' + iy
    char b1 = static_cast<char>(97 + fx);             // 'a' + fx
    char b2 = static_cast<char>(49 + fy);             // '1' + fy

    return string({a1, a2, b1, b2});
  };

  const string fen = utils::getFen(args, START_FEN);
  const Depth depth = utils::getDepth(args, 2);
  const string outputFile = utils::getOutputFile(args, "inp.txt");
  
  std::ofstream out(outputFile);
  ChessBoard pos(fen);
  MoveList myMoves = generateMoves(pos);
  MoveArray movesArray;
  myMoves.getMoves(pos, movesArray);

  for (const auto move : movesArray)
  {
    pos.makeMove(move);
    const auto current = bulkCount(pos, depth - 1);
    out << moveName(move) << " : " << current << '\n';
    pos.unmakeMove();
  }

  out.close();
}

static void
staticEval(const vector<string>& args)
{
  // elsa fen <fen> static

  const string fen = utils::getFen(args, START_FEN);
  ChessBoard pos(fen);

  MoveList myMoves = generateMoves(pos);
  MoveArray movesArray;
  myMoves.getMoves(pos, movesArray);
  orderMoves(pos, movesArray, MType::CAPTURES, 0);
  printMovelist(movesArray, pos);

  Score eval = evaluate<true>(pos);
  cout << "Score = " << eval << endl;
}

static void
isDrawCheck(const vector<string>& args)
{
  // elsa isDraw [fen <fen>]

  const string fen = utils::getFen(args, START_FEN);
  ChessBoard pos(fen);

  cout << "Fen = " << fen << '\n';

  // Mirror the search gate exactly (single_thread.cpp:51, :331): the recognizer
  // is consulted only on non-terminal positions with no capture available for
  // the side to move. Report which branch the position falls into.
  const MoveList moves = generateMoves(pos);

  if (!moves.anyMove())
  {
    cout << (moves.checkers ? "Terminal: checkmate" : "Terminal: stalemate")
         << "  (recognizer not consulted)\n";
    return;
  }

  if (moves.exists<MType::CAPTURES>(pos))
  {
    cout << "Capture available  (search skips the recognizer here)\n";
    return;
  }

  const bool draw = isTheoreticalDraw(pos);
  cout << "isTheoreticalDraw = " << (draw ? "true   (theoretical draw)"
                                          : "false  (not a theoretical draw)")
       << '\n';
}

static void
perpetualCheck(const vector<string>& args)
{
  // elsa perpetual [fen <fen>] [nodes <n>] [maxply <n>] [step <n>] [hist]
  //                 [nocache] [cache <entries>]
  //                 [order near|far|heavy|heavynear|none]
  //                 [evorder capture|flee|both|approach|none]
  //
  // Standalone driver for the perpetual-check prover. Not wired into search --
  // this exists to answer "does the proof work, and what does it cost?" in
  // isolation, before the gate question is put to an arena.
  //
  // Runs the prover once per ply cap, deepening by `step`, the way iterative
  // deepening does -- so the growth of the proof tree is readable off the
  // table rather than inferred from a single terminal number. `hist` adds the
  // per-ply node histogram of the deepest run.
  //
  // The cache is on by default; `nocache` turns it off for an A/B, and `cache`
  // caps how many distinct positions it holds. Each ply cap builds its own
  // table, which keeps the runs independent and the table above readable.
  // Carrying one across caps would be sound -- a FALSE entry stores the ply
  // ROOM it was established with, not the cap -- but it is deliberately not
  // done here, because it would make every row depend on the rows above it.
  //
  // `order` picks how the attacker's checks are sorted -- see CheckOrder. It
  // can only move the cost, never the verdict, so a run where two orders
  // disagree on `proven` at the SAME cap is a bug and not a result. Judge an
  // ordering on the `nocache` numbers: with the cache on, a bad order is partly
  // absorbed by memoising the subtrees it wastes, which flatters it.
  //
  // `evorder` is the same knob on the other layer -- see EvasionOrder. The
  // prediction was that it would earn little, on the grounds that an AND node
  // only exits early when an evasion BREAKS OUT and the deep search is made of
  // nodes where none does. That was wrong, and instructively so: `approach`
  // buys a large multiple, and the win shows up in `ghi%`, not in the branching
  // factor. Finding the refutation sooner means finding it WITHOUT closing a
  // repetition cycle, so the proof is storable instead of path-bound. Ordering
  // here is feeding the cache, not pruning the tree.
  //
  // The standalone defaults are `near` + `approach`. Both were beaten by their
  // opposites at some caps before the full ladder was run, so re-measure rather
  // than trusting them on a new position.

  const string fen = utils::getFen(args, START_FEN);
  ChessBoard pos(fen);

  auto intArg = [&](const string& flag, long long fallback) -> long long {
    const string v = utils::argValue(args, flag);
    if (v.empty()) return fallback;
    try { return std::stoll(v); } catch (...) { return fallback; }
  };

  const uint64_t nodeBudget = uint64_t(intArg("nodes", (long long)PERPETUAL_MAX_NODES));
  const int      plyCap     = int(intArg("maxply", PERPETUAL_MAX_PLY));
  const int      step       = std::max(1, int(intArg("step", 1)));
  const uint64_t cacheCap   = uint64_t(intArg("cache", (long long)PERPETUAL_MAX_CACHE));

  auto hasFlag = [&](const string& flag) {
    return std::find(args.begin(), args.end(), flag) != args.end();
  };

  const bool showHist = hasFlag("hist");
  const bool useCache = !hasFlag("nocache");

  const string orderArg = utils::argValue(args, "order");
  const CheckOrder order = checkOrderFromName(orderArg, CheckOrder::NEAR);

  const string evArg = utils::argValue(args, "evorder");
  const EvasionOrder evasion = evArg == "capture" ? EvasionOrder::CAPTURE
                             : evArg == "flee"    ? EvasionOrder::FLEE
                             : evArg == "both"    ? EvasionOrder::BOTH
                             : evArg == "none"    ? EvasionOrder::NONE
                             :                      EvasionOrder::APPROACH;

  cout << "Fen = " << fen << endl;
  cout << "Side to move (the attacker) = "
       << (pos.color == WHITE ? "white" : "black") << endl;
  cout << "Node budget per run = " << nodeBudget << endl;
  cout << "Cache = " << (useCache ? "on" : "off");
  if (useCache)
    cout << ", capped at " << cacheCap << " positions";
  cout << endl;
  cout << "Check order = "
       << (order == CheckOrder::NONE       ? "movegen order"
         : order == CheckOrder::NEAR       ? "nearest king first"
         : order == CheckOrder::HEAVY      ? "heaviest checker first"
         : order == CheckOrder::HEAVY_NEAR ? "heaviest checker first, then nearest king"
         :                                   "farthest from king first") << endl;
  cout << "Evasion order = "
       << (evasion == EvasionOrder::NONE    ? "movegen order"
         : evasion == EvasionOrder::CAPTURE ? "capture the checker first"
         : evasion == EvasionOrder::FLEE    ? "farthest from checker first"
         : evasion == EvasionOrder::APPROACH ? "nearest the checker first"
         :                                    "capture, then farthest") << endl << endl;

  cout << " | plyCap |        nodes |  ratio |  ebf | maxPly |      cached | hit% | ghi% | proven | budget |     time |" << endl;
  cout << " |--------|--------------|--------|------|--------|-------------|------|------|--------|--------|----------|" << endl;

  PerpetualStats deepest;
  uint64_t prevNodes = 0;

  for (int cap = step; cap <= plyCap; cap += step)
  {
    PerpetualStats st;
    const perf_clock start  = perf::now();
    const bool       proven =
      provesPerpetual(pos, st, nodeBudget, cap, useCache, cacheCap, order, evasion);
    const double     secs   = perf_time(perf::now() - start).count();

    // Growth per deepening step, and the same figure normalised to one ply so
    // caps taken with step > 1 stay comparable.
    const double ratio = prevNodes ? double(st.nodes) / double(prevNodes) : 0.0;
    const double ebf   = ratio > 0.0 ? std::pow(ratio, 1.0 / double(step)) : 0.0;

    // Share of node visits the cache answered, and the share of proofs the
    // graph-history rule refused to store. When the first disappoints, the
    // second is the place to look before blaming the hash.
    const uint64_t visits = st.nodes + st.cacheHits;
    const uint64_t proofs = st.cacheStores + st.truePathBound;
    const double   hitPct = visits ? 100.0 * double(st.cacheHits) / double(visits) : 0.0;
    const double   ghiPct = proofs ? 100.0 * double(st.truePathBound) / double(proofs) : 0.0;

    printf(" | %6d | %12llu | %6.2f | %4.2f | %6d | %11llu | %4.1f | %4.1f | %-6s | %-6s | %7.3fs |\n",
           cap, (unsigned long long)st.nodes, ratio, ebf, st.maxPly,
           (unsigned long long)st.cacheEntries, hitPct, ghiPct,
           proven ? "TRUE" : "-", st.budgetHit ? "hit" : "-", secs);
    fflush(stdout);

    deepest   = st;
    prevNodes = st.nodes;

    // Once proven, deeper caps can only re-derive the same proof.
    if (proven) break;
  }

  if (showHist)
  {
    cout << endl << " per-ply node histogram (deepest run)" << endl;
    cout << " |  ply |        nodes |  ratio | node |" << endl;
    cout << " |------|--------------|--------|------|" << endl;
    for (int p = 0; p <= deepest.maxPly and p < PERPETUAL_PLY_LIMIT; ++p)
    {
      const uint64_t n = deepest.nodesAtPly[p];
      if (n == 0) continue;
      const uint64_t prev  = p > 0 ? deepest.nodesAtPly[p - 1] : 0;
      const double   ratio = prev ? double(n) / double(prev) : 0.0;
      printf(" | %4d | %12llu | %6.2f | %-4s |\n",
             p, (unsigned long long)n, ratio, (p % 2 == 0) ? "OR" : "AND");
    }
  }
}

static void
readyOk()
{
  // Argument : elsa readyOk
  ChessBoard pos(START_FEN);

  if (bulkCount(pos, 3) == 8902)
    puts("Ready Ok!");
  else
    puts("Ready Not Ok!");
}

void
task(const vector<string>& args)
{
  if (args.empty())
  {
    puts("No Task Found!");
    puts("Type : \'elsa help\' to view command list.\n");
    return;
  }

  // Command map that associates commands with their handler functions
  const std::unordered_map<string, std::function<void(const vector<string>&)>> commandMap = {
    {"help",     [](const auto&){ helper(); }},
    {"accuracy", [](const auto&){ accuracyTest(); }},
    {"speed",    [](const auto&){ speedTest(); }},
    {"go",       [](const auto& arguments){ directSearch(arguments); }},
    {"count",    [](const auto& arguments){ nodeCount(arguments); }},
    {"movegen",  [](const auto& arguments){ debugMoveGenerator(arguments); }},
    {"static",   [](const auto& arguments){ staticEval(arguments); }},
    {"bestmove",  [](const auto& arguments){ bestMoveSearch(arguments); }},
    {"readyOk",   [](const auto&){ readyOk(); }},
    {"isDraw",    [](const auto& arguments){ isDrawCheck(arguments); }},
    {"perpetual", [](const auto& arguments){ perpetualCheck(arguments); }},
    {"tune",      [](const auto& arguments){ tuneEval(arguments); }},
    {"egvalidate", [](const auto& arguments){ validateEndgame(arguments); }}
  };

  // Search for any command in the args
  string foundCommand;
  for (const auto& arg : args) {
    if (commandMap.find(arg) != commandMap.end()) {
      foundCommand = arg;
      break;
    }
  }

  if (!foundCommand.empty()) {
    const auto it = commandMap.find(foundCommand);
    it->second(args);
  } else {
    puts("No Valid Task!");
    puts("Type : \'elsa help\' to view command list.\n");
  }
}

