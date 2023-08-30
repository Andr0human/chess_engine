
#include "play.h"

static string  inputfile;
static string outputfile;
static std::ofstream logger;

static void
WriteToFile(const string filename, const string message)
{
    std::ofstream out(filename);

    out << message;

    out.close();
}

static void
WriteToFile(std::ostream& writer, const string& data)
{
    writer << data << endl;
}

static void
GetInput(string& __s)
{
    const auto valid_args_found = [&__s] ()
    {
        // To read from the file
        std::ifstream infile(inputfile);
        
        getline(infile, __s);
        infile.close();
        
        return !__s.empty();
    };


    // Break between each read
    const auto WAIT_TIME_PER_CYCLE = std::chrono::microseconds(50);

    // Searching for a valid commandline
    while (valid_args_found() == false)
        std::this_thread::sleep_for(WAIT_TIME_PER_CYCLE);
}


static void
WriteSearchResult()
{
    const Move MOVE_FILTER = 2097151;
    const auto& [move, eval] = info.LastIterationResult();
    const double in_decimal = static_cast<double>(eval) / 100;

    string result = std::to_string(move & MOVE_FILTER) + string(" ")
                  + std::to_string(in_decimal);

    WriteToFile(outputfile, result);
}

static void
ReadInitCommands(const vector<string>& init_args, PlayBoard& __pos)
{
    size_t index = 0;

    for (const string& arg : init_args)
    {
        if (arg == "input")
        {
            puts("New Input path found!");
            inputfile = init_args[index + 1];
        }
        else if (arg == "output")
        {
            puts("Output path found!");
            outputfile = init_args[index + 1];
        }
        else if (arg == "log")
        {
            puts("Logger Found!");
            logger.open(init_args[index + 1]);
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
    std::ostream& writer = logger.is_open() ? logger : std::cout;

    for (const string& arg : args)
    {   
        if (arg == "go")
        {
            WriteToFile(writer, "go command found!");
            position.ReadySearch();
        }
        else if (arg == "moves")
        {
            WriteToFile(writer, "pre_moves found for position!");
            writer << "pre_move -> " << args[index + 1] << endl;
            position.AddPremoves(args, index + 1);
        }
        else if (arg == "time")
        {
            WriteToFile(writer, "time command found!");
            writer << "Time for search : " << args[index + 1] << " sec." << endl;
            position.SetMoveTime(stod(args[index + 1]));
        }
        else if (arg == "depth")
        {
            WriteToFile(writer, "depth command found!");
            position.SetDepth(stoi(args[index + 1]));
        }
        else if (arg == "quit")
        {
            WriteToFile(writer, "quit command found!");
            position.ReadyQuit();
        }

        index++;
    }
}


static void
ExecuteLateCommands(PlayBoard& board)
{
    std::ostream& writer = logger.is_open() ? logger : std::cout;

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
        WriteToFile(writer, string("Fen -> ") + fen);
        __pos.AddPreviousBoardPositions(board.GetPreviousHashkeys());

        WriteToFile(writer, "Start Search");
        Search(__pos, MAX_DEPTH, board.GetMoveTime(), logger);
        WriteToFile(writer, "End   Search");
        board.SearchDone();

        if (logger.is_open())
            WriteToFile(writer, info.GetSearchResult(__pos));

        WriteSearchResult();

        Move chosen_move = info.LastIterationResult().first;
        board.AddPremoves(chosen_move);
        TT.Clear();

        WriteToFile(logger, "After MakeMove");
        WriteToFile(logger, board.Fen());
    }
}


void
Play(const vector<string>& args)
{
    puts("PLAY mode started!");

    std::thread input_thread;
    PlayBoard board;

    ReadInitCommands(args, board);

    // Used to store the commands by user
    string input_string;

    while (true)
    {
        input_thread = std::thread(GetInput, std::ref(input_string));
        input_thread.join();

        logger << "Input recieve -> " << input_string << endl;
        const auto commands = base_utils::Split(input_string, ' ');
        ReadCommands(commands, board);

        // Empty the input_string
        WriteToFile(inputfile, "");
        input_string.clear();

        ExecuteLateCommands(board);

        if (board.DoQuit())
            break;
    }

    puts("PLAY mode ended!");
}

