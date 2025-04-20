#include "task.h"
#include "search.h"
#include "single_thread.h"
#include <unordered_map>
#include <functional>

void
init()
{
  perf_clock start = perf::now();
  plt::init();

  if constexpr (USE_TT) {
    tt.resize(0);
  }
  perf_time dur = perf::now() - start;
  const auto it = dur.count();

  bool sec = (it >= 1);
  cout << "Table Gen. took " << (sec ? it : it * 1000)
        << (sec ? " s.\n" : " ms.") << endl;

  if constexpr (USE_TT) {
    cout << "Transposition Table Size = " << tt.size()
        << "\n\n" << std::flush;
  }
}

static vector<TestPosition>
getTestPositions(const std::string& filename, const std::string& testType)
{
  std::ifstream infile;
  infile.open("../Utility/" + filename);

  string str;
  vector<TestPosition> testPositions;

  while (getline(infile, str))
  {
    if (str == "") break;
    testPositions.emplace_back(TestPosition(str, testType));
  }

  infile.close();
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

  const auto positions = getTestPositions("suite_2", "accuracy");
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
  puts("** elsa count <fen> <depth>\n");

  puts("** For Evaluating a position, type:\n");
  puts("** elsa go <fen> <search_time>\n");

  puts("** For debugging movegenerator, type:\n");
  puts("** elsa debug <fen> <depth> <output_file_name>\n");

  puts("/**************************************************/\n\n");
}

static void
speedTest()
{
  // Argument : elsa speed

  const auto positions = getTestPositions("suite_1", "speed");
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
directSearch(const vector<string> &args)
{
  // Argument : elsa go <fen> <search_time>

  const size_t n = args.size();
  const string fen = n > 1 ? args[1] : START_FEN;

  const double searchTime = (n >= 3) ?
    std::stod(args[2]) : static_cast<double>(DEFAULT_SEARCH_TIME);

  ChessBoard primary = fen;
  cout << primary.visualBoard() << endl;

  search(primary, MAX_DEPTH, searchTime);
}

static void
nodeCount(const vector<string> &args)
{
  // Argument : elsa count <fen> <depth>

  const size_t n = args.size();
  const string fen = n > 1 ? args[1] : START_FEN;
  Depth depth = n > 2 ? stoi(args[2]) : 6;
  ChessBoard pos(fen);

  cout << "Fen = " << fen << '\n';
  cout << "Depth = " << depth << "\n" << endl;

  const auto &[nodes, t] = perf::run_algo(bulkCount, pos, depth);

  cout << "Nodes(single-thread) = " << nodes << '\n';
  cout << "Time (single-thread) = " << t << " sec.\n";
  cout << "Speed(single-thread) = " << static_cast<double>(nodes) / (t * 1e6)
        << " M nodes/sec.\n" << endl;

  // const auto &[nodes2, t2] = perf::run_algo(bulk_MultiCount, board, depth);

  // cout << "Nodes(multi- thread) = " << nodes2 << endl;
  // cout << "Speed(multi- thread) = " << nodes2 / (t2 * 1'000'000)
  //      << " M nodes/sec." << endl;
  // cout << "Threads Used = " << threadCount << endl;
}

static void
debugMoveGenerator(const vector<string> &args)
{
  // Argument : elsa debug <fen> <depth> <output_file_name>

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

  const auto n = args.size();
  const auto fen = n > 1 ? args[1] : START_FEN;
  const auto dep = n > 2 ? stoi(args[2]) : 2;
  const auto fn = n > 3 ? args[3] : string("inp.txt");
  
  std::ofstream out(fn);
  ChessBoard pos(fen);
  MoveList myMoves = generateMoves(pos);
  MoveArray movesArray;
  myMoves.getMoves(pos, movesArray);

  for (const auto move : movesArray)
  {
    pos.makeMove(move);
    const auto current = bulkCount(pos, dep - 1);
    out << moveName(move) << " : " << current << '\n';
    pos.unmakeMove();
  }

  out.close();
}

static void
staticEval(const vector<string>& args)
{
  // Argument : elsa static <fen>
  if (args.size() == 1)
  {
    puts("No fen provided!");
    return;
  }

  ChessBoard pos(args[1]);

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
task(int argc, char *argv[])
{
  const vector<string> argumentList =
    utils::extractArgumentList(argc, argv);

  if (argumentList.empty())
  {
    puts("No Task Found!");
    puts("Type : \'elsa help\' to view command list.\n");
    return;
  }

  const string& command = argumentList[0];

  // Command map that associates commands with their handler functions
  const std::unordered_map<string, std::function<void(const vector<string>&)>> commandMap = {
    {"help",     [](const auto&){ helper(); }},
    {"accuracy", [](const auto&){ accuracyTest(); }},
    {"speed",    [](const auto&){ speedTest(); }},
    {"go",       [](const auto& args){ directSearch(args); }},
    {"play",     [](const auto& args){ play(args); }},
    {"count",    [](const auto& args){ nodeCount(args); }},
    {"debug",    [](const auto& args){ debugMoveGenerator(args); }},
    {"static",   [](const auto& args){ staticEval(args); }},
    {"readyOk",  [](const auto&){ readyOk(); }}
  };

  // Find and execute the command
  const auto it = commandMap.find(command);
  if (it != commandMap.end()) {
    it->second(argumentList);
  } else {
    puts("No Valid Task!");
  }
}

