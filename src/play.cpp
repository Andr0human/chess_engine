
#include "play.h"

static string  inputFile;
static string outputFile;

static std::ostream* logger;


static void
writeLog(string message)
{
  *logger << message << endl;
}


static void
getInput(string& __s, int validQuery)
{
  constexpr size_t INPUT_SIZE = 50;
  ifstream inputReader(inputFile);

  if (!inputReader.is_open())
  {
    writeLog("No Input File found. Terminating!");
    // Terminate the program
    exit(EXIT_FAILURE);
  }

  const auto validArgsFound = [&] ()
  {
    inputReader.clear();
    inputReader.seekg(0, std::ios::beg);

    getline(inputReader, __s);

    if (__s.length() != INPUT_SIZE)
      return false;

    int query = 0;
    for (int i = 0; ('0' <= __s[i]) and (__s[i] <= '9'); i++)
      query = 10 * query + (__s[i] - '0');

    return query == validQuery;
  };

  // Break between each read
  constexpr auto WAIT_TIME_PER_CYCLE = std::chrono::microseconds(50);

  // Searching for a valid commandline
  while (!validArgsFound())
    std::this_thread::sleep_for(WAIT_TIME_PER_CYCLE);
}


static void
writeSearchResult(PlayBoard& pb)
{
  using std::to_string;
  constexpr size_t OUTPUT_SIZE = 30;

  const auto& [move, eval] = info.lastIterationResult();
  const double inDecimal  = double(eval) / 100;

  string result =
    to_string(filter(move)) + " " + to_string(inDecimal) + " " + to_string(pb.query());

  result.append(OUTPUT_SIZE - result.size(), ' ');
  ofstream outputWriter(outputFile);
  outputWriter << result;
  outputWriter.close();
}


static void
readInitCommands(const vector<string>& initArgs, PlayBoard& position)
{
  size_t index = 0;

  for (const string& arg : initArgs)
  {
    if (arg == "input")
    {
      inputFile = initArgs[index + 1];
      writeLog("Input  path found!");
    }
    else if (arg == "output")
    {
      outputFile = initArgs[index + 1];
      writeLog("Output path found!");
    }
    else if (arg == "log")
    {
      logger = new ofstream(initArgs[index + 1]);
      writeLog("Logger path found!");
    }
    else if (arg == "position")
    {
      const string fen = utils::strip(initArgs[index + 1], '"');
      position.setNewPosition(fen);

      writeLog("New position found!");
      writeLog(position.visualBoard());
    }
    else if (arg == "threads")
    {
      writeLog("threads command found!");
      position.setThreads(std::stoi(initArgs[index + 1]));
    }
    ++index;
  }
}


static void
readCommands(const vector<string>& args, PlayBoard& position)
{
  using std::stoi;
  using std::stod;

  size_t index = 0;

  for (const string& arg : args)
  {   
    if (arg == "go")
    {
      writeLog("Go command found!");
      position.readySearch();
    }
    else if (arg == "moves")
    {
      writeLog("premove -> " + args[index + 1]);
      position.addPremoves(args, index + 1);
    }
    else if (arg == "time")
    {
      writeLog("Time for search : " + args[index + 1] + " sec.");
      position.setMoveTime(stod(args[index + 1]));
    }
    else if (arg == "depth")
    {
      writeLog("Depth command found!");
      position.setDepth(stoi(args[index + 1]));
    }
    else if (arg == "quit")
    {
      writeLog("Quit command found!");
      position.readyQuit();
    }

    index++;
  }
}


static void
executeLateCommands(PlayBoard& board)
{
  // If prev moves to be made on board
  if (board.preMovesExist())
    board.playPreMoves();

  // Start searching for best move in current position (go)
  if (board.doSearch())
  {
    string fen = board.fen();
    ChessBoard pos(fen);
    writeLog("Fen -> " + fen);
    pos.addPreviousBoardPositions(board.getPreviousHashkeys());

    writeLog("Start Search!");
    search(pos, MAX_DEPTH, board.getMoveTime(), *logger);
    writeLog("End Search!");

    board.searchDone();

    writeSearchResult(board);

    board.updateQuery();

    Move chosen_move = info.lastIterationResult().first;
    board.addPremoves(chosen_move);
    tt.clear();
  }
}


void
play(const vector<string>& args)
{
  puts("PLAY mode started!");

  std::thread inputThread;
  PlayBoard board;

  // Init Loggers
  logger = &std::cout;

  readInitCommands(args, board);

  // Used to store the commands by user
  string inputString;

  while (true)
  {
    inputThread = std::thread(getInput, std::ref(inputString), board.query());
    inputThread.join();

    writeLog(string("Input recieve -> ") + inputString);
    const auto commands = utils::split(inputString, ' ');
    readCommands(commands, board);

    // Empty the Input file and string
    inputString.clear();

    executeLateCommands(board);

    if (board.doQuit())
      break;
  }

  puts("PLAY mode ended!");
}

