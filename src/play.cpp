
#include "play.h"

static string  inputFile;
static string outputFile;

static std::ostream* logger;


static void
WriteLog(string message)
{
    *logger << message << endl;
}


static void
GetInput(string& __s, int validQuery)
{
    constexpr size_t INPUT_SIZE = 50;
    ifstream inputReader(inputFile);

    if (!inputReader.is_open())
    {
        WriteLog("No Input File found. Terminating!");
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
WriteSearchResult(PlayBoard& pb)
{
    using std::to_string;
    constexpr size_t OUTPUT_SIZE = 30;

    const auto& [move, eval] = info.LastIterationResult();
    const double in_decimal  = double(eval) / 100;

    string result =
        to_string(filter(move)) + " " + to_string(in_decimal) + " " + to_string(pb.Query());

    result.append(OUTPUT_SIZE - result.size(), ' ');
    ofstream outputWriter(outputFile);
    outputWriter << result;
    outputWriter.close();
}


static void
ReadInitCommands(const vector<string>& init_args, PlayBoard& position)
{
    size_t index = 0;

    for (const string& arg : init_args)
    {
        if (arg == "input")
        {
            inputFile = init_args[index + 1];
            WriteLog("Input  path found!");
        }
        else if (arg == "output")
        {
            outputFile = init_args[index + 1];
            WriteLog("Output path found!");
        }
        else if (arg == "log")
        {
            logger = new ofstream(init_args[index + 1]);
            WriteLog("Logger path found!");
        }
        else if (arg == "position")
        {
            const string fen = base_utils::Strip(init_args[index + 1], '"');
            position.SetNewPosition(fen);

            WriteLog("New position found!");
            WriteLog(position.VisualBoard());
        }
        else if (arg == "threads")
        {
            WriteLog("threads command found!");
            position.SetThreads(std::stoi(init_args[index + 1]));
        }
        ++index;
    }
}


static void
ReadCommands(const vector<string>& args, PlayBoard& position)
{
    using std::stoi;
    using std::stod;

    size_t index = 0;

    for (const string& arg : args)
    {   
        if (arg == "go")
        {
            WriteLog("Go command found!");
            position.ReadySearch();
        }
        else if (arg == "moves")
        {
            WriteLog("premove -> " + args[index + 1]);
            position.AddPremoves(args, index + 1);
        }
        else if (arg == "time")
        {
            WriteLog("Time for search : " + args[index + 1] + " sec.");
            position.SetMoveTime(stod(args[index + 1]));
        }
        else if (arg == "depth")
        {
            WriteLog("Depth command found!");
            position.SetDepth(stoi(args[index + 1]));
        }
        else if (arg == "quit")
        {
            WriteLog("Quit command found!");
            position.ReadyQuit();
        }

        index++;
    }
}


static void
ExecuteLateCommands(PlayBoard& board)
{
    // If prev moves to be made on board
    if (board.PremovesExist())
    {
        board.PlayPreMoves();
    }

    // Start searching for best move in current position (go)
    if (board.DoSearch())
    {
        string fen = board.Fen();
        ChessBoard __pos(fen);
        WriteLog("Fen -> " + fen);
        __pos.AddPreviousBoardPositions(board.GetPreviousHashkeys());

        WriteLog("Start Search!");
        Search(__pos, MAX_DEPTH, board.GetMoveTime(), *logger);
        WriteLog("End Search!");

        board.SearchDone();

        WriteSearchResult(board);

        board.UpdateQuery();

        Move chosen_move = info.LastIterationResult().first;
        board.AddPremoves(chosen_move);
        TT.Clear();
    }
}


void
Play(const vector<string>& args)
{
    puts("PLAY mode started!");

    std::thread input_thread;
    PlayBoard board;

    // Init Loggers
    logger = &std::cout;

    ReadInitCommands(args, board);

    // Used to store the commands by user
    string input_string;

    while (true)
    {
        input_thread = std::thread(GetInput, std::ref(input_string), board.Query());
        input_thread.join();

        WriteLog(string("Input recieve -> ") + input_string);
        const auto commands = base_utils::Split(input_string, ' ');
        ReadCommands(commands, board);

        // Empty the Input file and string
        input_string.clear();

        ExecuteLateCommands(board);

        if (board.DoQuit())
            break;
    }

    puts("PLAY mode ended!");
}

