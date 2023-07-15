
#include "play.h"

static string  inputfile;
static string outputfile;

void
write_to_file(const string filename, const string message)
{
    std::ofstream out(filename);

    out << message;

    out.close();
}


void
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
    const auto WAIT_TIME_PER_CYCLE = std::chrono::microseconds(3);

    // Searching for a valid commandline
    while (valid_args_found() == false)
        std::this_thread::sleep_for(WAIT_TIME_PER_CYCLE);
}


void
read_init_commands(const vector<string>& init_args, playboard& __pos)
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
        else if (arg == "position")
        {
            const string fen = base_utils::strip(init_args[index + 1], '"');
            __pos.set_new_position(fen);

            puts("New position found!");
            __pos.show();
        }
        else if (arg == "threads")
        {
            puts("threads command found!");
            __pos.set_thread(std::stoi(init_args[index + 1]));
        }

        ++index;
    }
}


void
read_commands(const vector<string>& args, playboard& __pos)
{
    using std::stoi;
    using std::stod;

    size_t index = 0;

    for (const string& arg : args)
    {   
        if (arg == "go")
        {
            puts("go command found!");
            __pos.ready_search();
        }
        else if (arg == "moves")
        {
            puts("moves command found!");
            __pos.play_moves_before_search(args, index + 1);
        }
        else if (arg == "time")
        {
            puts("movetime command found!");
            __pos.set_movetime(stod(args[index + 1]));
        }
        else if (arg == "depth")
        {
            puts("depth command found!");
            __pos.set_depth(stoi(args[index + 1]));
        }
        else if (arg == "quit")
        {
            puts("quit command found!");
            __pos.ready_quit();
        }

        index++;
    }
}


void
execute_commands(playboard& board)
{
    /* if (board.integrity_check() == false)
    {
        puts("Integrity Check Failed");

        puts("Board => ");
        for (int i = 0; i < 64; i++)
        {
            if (board.board[i] != 0)
                cout << i << " -> " << endl;
        }

        puts("Pieces => ");
        for (int i = 0; i < 16; i++)
            cout << i << " -> " << endl;

        board.show();
        system("pause");
    } */






    // If prev moves to be made on board
    if (board.to_play_moves())
        board.play_moves();

    // Start searching for best move in current position (go)
    if (board.to_search())
    {
        ChessBoard __pos = board.fen();

        __pos.show();
        __pos.add_prev_board_positions(board.get_prev_hashkeys());

        search_iterative(__pos, maxDepth, board.get_movetime());
        // Show_Searched_Info(__pos);
        info.show_search_results(__pos);
        board.search_done();

        const MoveType MOVE_FILTER = 2097151;
        const auto& [move, eval] = info.last_iter_result();
        const double in_decimal = static_cast<double>(eval) / 100;

        string result = std::to_string(move & MOVE_FILTER) + string(" ")
                      + std::to_string(in_decimal);

        write_to_file(outputfile, result);

        board.play_moves(vector<MoveType>{move});
        TT.clear();
    }

}


void
play(const vector<string>& args)
{
    puts("PLAY mode started!");

    std::thread input_thread;
    playboard board;

    read_commands(args, board);

    read_init_commands(args, board);

    // Used to store the commands by user
    string input_string;

    while (true)
    {
        input_thread = std::thread(get_input, std::ref(input_string));
        input_thread.join();

        const auto commands = base_utils::split(input_string, ' ');
        read_commands(commands, board);

        // Empty the input_string
        write_to_file(inputfile, "");
        input_string.clear();

        execute_commands(board);

        if (board.do_quit())
            break;
    }

    puts("PLAY mode ended!");
}

