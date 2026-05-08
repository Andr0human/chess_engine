
#include <unordered_map>
#include <functional>
#include "task.h"
#include "search.h"
#include "single_thread.h"
#include "test_positions.h"

void
init(const vector<string>& args)
{
  perf_clock start = perf::now();
  plt::init();

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

    for (const auto& pos : positions)
    {
      const auto depth = pos.maxDepth();
      if (!pos.test(bulkCount, depth))
        return false;
      
      cout << "Position " << (posNo++) << " passed!" << endl;
    }

    return true;
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
  const auto testSuccess = runTests(positions);

  if (testSuccess) return;

  cout << "Accuracy Test Failed!!\n" <<
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
movegenSpeedSuite(
  const std::vector<std::string>& rawSuite,
  const std::string& suiteType,
  const std::string& suiteLabel,
  int iterations
) {
  const auto positions = getTestPositions(rawSuite, suiteType);
  cout << "\n=== " << suiteLabel << " (" << positions.size()
       << " positions, " << iterations << " iter/pos) ===\n";

  uint64_t totalP1Ns = 0, totalP12Ns = 0;

  int positionNo = 1;
  for (const auto& tp : positions)
  {
    ChessBoard pos(tp.getFen());

    // Phase 1 only — accumulate countMoves() so the compiler can't elide.
    uint64_t p1Sink = 0;
    const auto p1Start = perf::now();
    for (int i = 0; i < iterations; ++i)
    {
      MoveList ml = generateMoves(pos);
      p1Sink += ml.countMoves();
    }
    const auto p1Ns = std::chrono::duration_cast<perf_ns_time>(
      perf::now() - p1Start
    ).count();

    // Phase 1 + Phase 2 — also materialize into MoveArray.
    uint64_t p12Sink = 0;
    const auto p12Start = perf::now();
    for (int i = 0; i < iterations; ++i)
    {
      MoveList ml = generateMoves(pos);
      MoveArray arr;
      ml.getMoves(pos, arr);
      p12Sink += arr.size();
    }
    const auto p12Ns = std::chrono::duration_cast<perf_ns_time>(
      perf::now() - p12Start
    ).count();

    totalP1Ns  += static_cast<uint64_t>(p1Ns);
    totalP12Ns += static_cast<uint64_t>(p12Ns);

    const double p1NsPerCall  = static_cast<double>(p1Ns)  / iterations;
    const double p12NsPerCall = static_cast<double>(p12Ns) / iterations;
    const double ratio        = p12NsPerCall / p1NsPerCall;

    cout << "position-" << positionNo++ << "\t"
         << "phase1: "  << std::fixed << std::setprecision(1) << p1NsPerCall << " ns/call"
         << "\tphase1+2: " << p12NsPerCall << " ns/call"
         << "\toverhead: "       << (p12NsPerCall - p1NsPerCall) << " ns"
         << "\toverhead ratio: " << std::setprecision(2) << ratio
         << '\n';

    if (p1Sink != p12Sink)
      cout << "  WARN: countMoves (" << p1Sink << ") != materialized ("
           << p12Sink << ")\n";
  }

  const double avgP1  = static_cast<double>(totalP1Ns)
                      / static_cast<double>(positions.size() * iterations);
  const double avgP12 = static_cast<double>(totalP12Ns)
                      / static_cast<double>(positions.size() * iterations);
  const double avgRatio = avgP12 / avgP1;

  cout << "Overall (" << suiteLabel << "):\n"
       << "  Phase 1 only     : " << std::fixed << std::setprecision(1)
       << avgP1  << " ns/call  (" << (1000.0 / avgP1)  << " M calls/sec)\n"
       << "  Phase 1 + Phase 2: " << avgP12 << " ns/call  ("
       << (1000.0 / avgP12) << " M calls/sec)\n"
       << "  Phase 2 overhead : " << (avgP12 - avgP1) << " ns/call\n"
       << "  Overhead ratio   : " << std::setprecision(2) << avgRatio << '\n';
}

static void
movegenSpeedTest()
{
  // Argument : elsa movegenspeed
  //
  // Benchmarks two regimes:
  //   (1) phase 1 only        — generateMoves(pos)
  //   (2) phase 1 + phase 2   — generateMoves(pos) + getMoves(pos, arr)
  // Run over both the speed suite (handpicked piece-density positions) and
  // the accuracy suite (perft positions — broader mix of game states).

  movegenSpeedSuite(test_data::speed::suite1,    "speed",    "speed::suite1",    2'000'000);
  movegenSpeedSuite(test_data::accuracy::suite1, "accuracy", "accuracy::suite1", 1'000'000);
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
    {"movegenspeed", [](const auto&){ movegenSpeedTest(); }},
    {"go",       [](const auto& arguments){ directSearch(arguments); }},
    {"count",    [](const auto& arguments){ nodeCount(arguments); }},
    {"movegen",  [](const auto& arguments){ debugMoveGenerator(arguments); }},
    {"static",   [](const auto& arguments){ staticEval(arguments); }},
    {"bestmove", [](const auto& arguments){ bestMoveSearch(arguments); }},
    {"readyOk",  [](const auto&){ readyOk(); }}
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

