
#include "play.h"

static string  inputfile;
static string outputfile;
static std::ofstream logger;

static void
write_to_file(const string filename, const string message)
{
    std::ofstream out(filename);

    out << message;

    out.close();
}

static void
write_to_file(std::ostream& writer, const string& data)
{
    writer << data << endl;
}

static void
get_input(string& __s)
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
write_search_result()
{
    const Move MOVE_FILTER = 2097151;
    const auto& [move, eval] = info.last_iter_result();
    const double in_decimal = static_cast<double>(eval) / 100;

    string result = std::to_string(move & MOVE_FILTER) + string(" ")
                  + std::to_string(in_decimal);

    write_to_file(outputfile, result);
}

static void
read_init_commands(const vector<string>& init_args, PlayBoard& __pos)
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
            const string fen = base_utils::strip(init_args[index + 1], '"');
            __pos.set_new_position(fen);

            puts("New position found!");
            cout << __pos.visual_board() << endl;
        }
        else if (arg == "threads")
        {
            puts("threads command found!");
            __pos.set_thread(std::stoi(init_args[index + 1]));
        }

        ++index;
    }
}


static void
read_commands(const vector<string>& args, PlayBoard& position)
{
    using std::stoi;
    using std::stod;

    size_t index = 0;
    std::ostream& writer = logger.is_open() ? logger : std::cout;

    for (const string& arg : args)
    {   
        if (arg == "go")
        {
            write_to_file(writer, "go command found!");
            position.ready_search();
        }
        else if (arg == "moves")
        {
            write_to_file(writer, "pre_moves found for position!");
            writer << "pre_move -> " << args[index + 1] << endl;
            position.add_premoves(args, index + 1);
        }
        else if (arg == "time")
        {
            write_to_file(writer, "time command found!");
            writer << "Time for search : " << args[index + 1] << " sec." << endl;
            position.set_movetime(stod(args[index + 1]));
        }
        else if (arg == "depth")
        {
            write_to_file(writer, "depth command found!");
            position.set_depth(stoi(args[index + 1]));
        }
        else if (arg == "quit")
        {
            write_to_file(writer, "quit command found!");
            position.ready_quit();
        }

        index++;
    }
}


static void
execute_late_commands(PlayBoard& board)
{
    std::ostream& writer = logger.is_open() ? logger : std::cout;

    // If prev moves to be made on board
    if (board.premoves_exist())
    {
        board.play_premoves();
    }

    // Start searching for best move in current position (go)
    if (board.do_search())
    {
        string fen = board.fen();
        ChessBoard __pos(fen);
        write_to_file(writer, string("Fen -> ") + fen);
        __pos.add_prev_board_positions(board.get_prev_hashkeys());

        write_to_file(writer, "Start Search");
        search_iterative(__pos, MAX_DEPTH, board.get_movetime(), logger);
        write_to_file(writer, "End   Search");
        board.search_done();

        if (logger.is_open())
            write_to_file(writer, info.get_search_results(__pos));

        write_search_result();

        Move chosen_move = info.last_iter_result().first;
        board.add_premoves(chosen_move);
        TT.clear();

        write_to_file(logger, "After MakeMove");
        write_to_file(logger, board.fen());
    }
}


void
play(const vector<string>& args)
{
    puts("PLAY mode started!");

    std::thread input_thread;
    PlayBoard board;

    read_init_commands(args, board);

    // Used to store the commands by user
    string input_string;

    while (true)
    {
        input_thread = std::thread(get_input, std::ref(input_string));
        input_thread.join();

        logger << "Input recieve -> " << input_string << endl;
        const auto commands = base_utils::split(input_string, ' ');
        read_commands(commands, board);

        // Empty the input_string
        write_to_file(inputfile, "");
        input_string.clear();

        execute_late_commands(board);

        if (board.do_quit())
            break;
    }

    puts("PLAY mode ended!");
}

