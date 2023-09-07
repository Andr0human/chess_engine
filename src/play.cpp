
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
ClearInputFile()
{
    std::ofstream clearFile(inputFile, std::ios::out | std::ios::trunc);
    if (clearFile.is_open()) {
        clearFile.close();
        WriteLog("File content cleared!");
    } else {
        WriteLog("Error: Could not clear the file!");
    }
}


static void
GetInput(string& __s)
{
    ifstream inputReader(inputFile);

    if (!inputReader.is_open())
    {
        WriteLog("No Input File found. Terminating!");
        // Terminate the program
        exit(EXIT_FAILURE);
    }

    const auto validArgsFound = [&] () {
        inputReader.seekg(0, std::ios::beg);
        return inputReader.peek() != EOF;
    };

    // Break between each read
    constexpr auto WAIT_TIME_PER_CYCLE = std::chrono::microseconds(50);

    // Searching for a valid commandline
    while (!validArgsFound())
        std::this_thread::sleep_for(WAIT_TIME_PER_CYCLE);

    getline(inputReader, __s);
}


static void
WriteSearchResult()
{
    const Move MOVE_FILTER = 2097151;
    const auto& [move, eval] = info.LastIterationResult();
    const double in_decimal = static_cast<double>(eval) / 100;

    string result = std::to_string(move & MOVE_FILTER) + string(" ")
                  + std::to_string(in_decimal) + string(" ")
                  + std::to_string(info.SearchTime());

    /* // Rewind to the beginning of the file before writing
    outputWriter.seekp(0);
    outputWriter << result;
    outputWriter.flush(); // Flush the output */

    ofstream outputWriter(outputFile);
    outputWriter << result;
    outputWriter.close();
}


static void
ReadInitCommands(const vector<string>& init_args, PlayBoard& __pos)
{
    size_t index = 0;

    for (const string& arg : init_args)
    {
        if (arg == "input")
        {
            puts("Input  path found!");
            inputFile = init_args[index + 1];
        }
        else if (arg == "output")
        {
            puts("Output path found!");
            outputFile = init_args[index + 1];
        }
        else if (arg == "log")
        {
            puts("Logger Found!");
            logger = new ofstream(init_args[index + 1]);
        }
        else if (arg == "position")
        {
            const string fen = base_utils::Strip(init_args[index + 1], '"');
            __pos.SetNewPosition(fen);

            puts("New position found!");
            cout << __pos.VisualBoard() << endl;
        }
        else if (arg == "threads")
        {
            puts("threads command found!");
            __pos.SetThreads(std::stoi(init_args[index + 1]));
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

        WriteLog("Start Log Search Result!");
        WriteLog(info.GetSearchResult(__pos));
        WriteLog("End   Log Search Result!");
        WriteSearchResult();

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
        input_thread = std::thread(GetInput, std::ref(input_string));
        input_thread.join();

        WriteLog(string("Input recieve -> ") + input_string);
        const auto commands = base_utils::Split(input_string, ' ');
        ReadCommands(commands, board);

        // Empty the Input file and string
        input_string.clear();
        ClearInputFile();

        ExecuteLateCommands(board);

        if (board.DoQuit())
            break;
    }

    puts("PLAY mode ended!");
}

