
#include "task.h"
#include "move_utils.h"
#include "search.h"
#include "single_thread.h"
#include "types.h"

void
Init()
{
  perf_clock start = perf::now();
  plt::Init();

  if constexpr (useTT) {
    TT.resize(0);
  }
  perf_time dur = perf::now() - start;
  const auto it = dur.count();

  bool sec = (it >= 1);
  cout << "Table Gen. took " << (sec ? it : it * 1000)
        << (sec ? " s.\n" : " ms.") << endl;

  if constexpr (useTT) {
    cout << "Transposition Table Size = " << TT.size()
        << "\n\n" << std::flush;
  }
}

vector<TestPosition>
getTestPositions(string filename, string testType)
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

void
AccuracyTest()
{
  const auto runTests = [] (const vector<TestPosition>& positions)
  {
    int posNo = 1;

    for (const auto& pos : positions)
    {
      const auto depth = pos.maxDepth();
      if (!pos.test(BulkCount, depth))
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
        if (pos.test(BulkCount, dep))
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

void
Helper() 
{
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


void
SpeedTest()
{
  const auto positions = getTestPositions("suite_1", "speed");
  cout << "Positions Found : " << positions.size() << '\n';
  
  int64_t totalTime = 0;
  Nodes  totalNodes = 0;
  const int    loop = 3;
  int positionNo    = 1;

  for (auto pos : positions)
  {
    Nodes  currentNodes = pos.nodeCount(1) * loop;
    uint64_t currentTime = pos.time(BulkCount, loop);

    totalNodes += currentNodes;
    totalTime  += currentTime;

    const uint64_t current_speed = currentNodes / currentTime;
    cout << "position-" << positionNo++ << "\t: " << current_speed << " M nodes/sec." << endl;
  }

  const uint64_t speed = totalNodes / totalTime;
  cout << "Single Thread Speed : " << speed << " M nodes/sec." << endl;
}

void
DirectSearch(const vector<string> &_args)
{
  const size_t __n = _args.size();
  const string fen = __n > 1 ? _args[1] : StartFen;

  const double search_time = (__n >= 3) ?
    std::stod(_args[2]) : static_cast<double>(DEFAULT_SEARCH_TIME);

  ChessBoard primary = fen;
  cout << primary.VisualBoard() << endl;

  Search(primary, MAX_DEPTH, search_time);
}

void
NodeCount(const vector<string> &_args)
{
  const size_t __n = _args.size();
  const string fen = __n > 1 ? _args[1] : StartFen;
  Depth depth  = __n > 2 ? stoi(_args[2]) : 6;
  ChessBoard _cb   = fen;

  cout << "Fen = " << fen << '\n';
  cout << "Depth = " << depth << "\n" << endl;

  const auto &[nodes, t] = perf::run_algo(BulkCount, _cb, depth);

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

void
DebugMoveGenerator(const vector<string> &_args)
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

  const auto __n = _args.size();
  const auto fen = __n > 1 ? _args[1] : StartFen;
  const auto dep = __n > 2 ? stoi(_args[2]) : 2;
  const auto _fn = __n > 3 ? _args[3] : string("inp.txt");
  
  std::ofstream out(_fn);
  ChessBoard _cb = fen;
  MoveList myMoves = GenerateMoves(_cb);
  MoveArray movesArray;
  myMoves.getMoves(_cb, movesArray);

  for (const auto move : movesArray)
  {
    _cb.MakeMove(move);
    const auto current = BulkCount(_cb, dep - 1);
    out << moveName(move) << " : " << current << '\n';
    _cb.UnmakeMove();
  }

  out.close();
}


static void
Level1(const vector<string>& args)
{
  if (args.size() == 1)
  {
    puts("No fen provided!");
    return;
  }

  ChessBoard pos(args[1]);

  MoveList myMoves = GenerateMoves(pos);
  MoveArray movesArray;
  myMoves.getMoves(pos, movesArray);
  OrderMoves(pos, movesArray, Sorts::CAPTURES);
  PrintMovelist(movesArray, pos);

  Score eval = Evaluate<true>(pos);
  cout << "Score = " << eval << endl;
}


void Task(int argc, char *argv[])
{
  const vector<string> argument_list =
    base_utils::ExtractArgumentList(argc, argv);

  string command = argument_list.empty() ? "" : argument_list[0];

  if (argument_list.empty())
  {
    puts("No Task Found!");
    puts("Type : \'elsa help\' to view command list.\n");
  }
  else if (command == "help")
  {
    Helper();
  }
  else if (command == "accuracy")
  {
    // Argument : elsa accuracy
    AccuracyTest();
  }
  else if (command == "speed")
  {
    // Argument : elsa speed
    SpeedTest();
  }
  else if (command == "go")
  {
    // Argument : elsa go "fen" allowed_search_time allowed_extra_time
    DirectSearch(argument_list);
  }
  else if (command == "play")
  {
    Play(argument_list);
  }
  else if (command == "count")
  {
    // Argument : elsa count {fen} {depth}
    NodeCount(argument_list);
  }
  else if (command == "debug")
  {
    DebugMoveGenerator(argument_list);
  }
  else if (command == "level1")
  {
    Level1(argument_list);
  }
  else
  {
    puts("No Valid Task!");
  }
}

