
#include "play.h"


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
        const string filename = "elsa_in";

        // To read from the file
        std::ifstream infile(filename);
        
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
read_commands(const string& __s, playboard& board)
{
    using std::stoi;
    using std::stod;

    std::vector<string> args = base_utils::split(__s, ' ');
    size_t index = 0;


    for (const string& arg : args)
    {   
        if (arg == "go")
        {
            puts("go command found!");
            board.ready_search();
        }
        else if (arg == "position")
        {
            puts("position command found!");
            
            const string fen = base_utils::strip(args[index + 1], '"');
            board.set_new_position(fen);
        }
        else if (arg == "moves")
        {
            puts("moves command found!");
            board.play_moves_before_search(args, index + 1);
        }
        else if (arg == "movetime")
        {
            puts("movetime command found!");
            board.set_movetime(stod(args[index + 1]));
        }
        else if (arg == "threads")
        {
            puts("threads command found!");
            board.set_thread(stoi(args[index + 1]));
        }
        else if (arg == "depth")
        {
            puts("depth command found!");
            board.set_depth(stoi(args[index + 1]));
        }
        else if (arg == "quit")
        {
            puts("quit command found!");
            board.ready_quit();
        }

        index++;
    }
}


void
execute_commands(playboard& board)
{
    // If prev moves to be made on board
    if (board.to_play_moves())
        board.play_moves();


    // Start searching for best move in current position (go)
    if (board.init_search())
    {
        chessBoard __pos = board.fen();

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

        write_to_file("elsa_out", result);

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

    // Used to store the commands by user
    string input_string;

    while (true)
    {
        input_thread = std::thread(get_input, std::ref(input_string));
        input_thread.join();

        read_commands(input_string, board);

        // Empty the input_string
        write_to_file("elsa_in", "");
        input_string.clear();

        execute_commands(board);

        if (board.do_quit())
            break;
    }

    puts("PLAY mode ended!");
}

