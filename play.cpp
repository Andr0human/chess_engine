

#include "play.h"

play_board pb;
std::ifstream inFile;
int moveNum = 0;



/*** CommandList to implement
 * go - To start searching for the best move

 * position - set the current position using fen
 * position [fen <FEN string> | startpos]

 * moves - play the current in move(s) in set position. (Do a legality-check first)
 * moves <move1> <move2> ... <movei>

 * movetime - time allocated for the search [default : 2 sec.]

 * threads - threads used for the search

 * depth - maxDepth to be searched [default : 36] 

 * quit - exits the program

***/


#ifndef READ_WRITE

bool readFile()
{
    string str;
    inFile.open(file_name + "_in.txt");
    getline(inFile, str);
    inFile.close();

    const size_t len = str.length();
    if (str[len - 2] != '-' || str[len - 1] != 'e') return false;

    pb.commandline  = str;
    return true;
}

void writeFile(string message, char file)
{
    std::ofstream outFile;
    if (file == 'i')
        outFile.open(file_name + "_in.txt");
    else
        outFile.open(file_name + "_out.txt");
    outFile << message;
    outFile.close();
}

void write_Execute_Result()
{
    std::ofstream outFile;
    outFile.open(file_name + "_out.txt");
    outFile << (info.best_move() & 2097151) << " " << info.last_eval() << " -out";
    outFile.close();
}

#endif

#ifndef PLAY

void start_game(const vector<string>& arg_list)
{
    const string fen = arg_list.size() >= 2 ? arg_list[1] : startFen;
    
    // TT.reIntialize(2'000'073, 10'073);
    puts("Play Mode Started!");
    cout << "FEN : " << fen << '\n';
    pb.init(fen);

    play();
}

int read_commands()
{
    std::vector<string> commands = split(pb.commandline, ' ');
    cout << "CommandLine : " << pb.commandline << endl;
    alloted_search_time = 2;
    alloted_extra_time = 0;
    threadCount = 4;
    int result = 0;


    for (size_t i = 0; i < commands.size(); i++) {
        if (commands[i] == "-q") {
            result |= quit;
        }
        else if (commands[i] == "-p") {
            result |= self_move;
        }
        else if (commands[i] == "-m") {
            result |= opp_move;
            pb.opponent_move = std::stoi(commands[i + 1]);
        }
        else if (commands[i] == "-tm") {
            if (i + 1 < commands.size())
                alloted_search_time = std::stod(commands[i + 1]);
            if (i + 2 < commands.size())
                alloted_extra_time  = std::stod(commands[i + 2]);
        }
        else if (commands[i] == "-th") {
            threadCount = std::stoi(commands[i + 1]);
        }
    }

    return result;
}

void execute_Commands(int command)
{
    cout << "Starting Fen : " << pb.board.fen() << endl;
    cout << "MoveNum : " << ++moveNum << endl;

    if (command & opp_move) {
        if (is_valid_move(pb.opponent_move, pb.board)) {
            cout << "Oppenent Move - " << print(pb.opponent_move, pb.board) << " [Valid]." << endl;
            pb.play_move(pb.opponent_move);
        } else {
            cout << "Opponent Move Invalid, Discarding!" << endl;
        }
    }

    if (command & self_move)
    {
        find_move_for_position();
        write_Execute_Result();
        pb.play_move(info.best_move() & 2097151);
        info.reset();
    }
    cout << endl;
    return;
}

void find_move_for_position()
{
    // perf::Timer pos("Find Move");
    chessBoard primary = pb.board;
    pb.generate_history_table();

    MakeMove_Iterative(primary, maxDepth, true);
    // MakeMove_MultiIterative(primary);
    // MakeMove_MultiIterative(primary, maxDepth, true);
    Show_Searched_Info(primary);
}

void getInput()
{
    while (true)
    {
        if (readFile())
            break;
        std::this_thread::sleep_for(std::chrono::microseconds(5));
    }
}


void play()
{
    std::thread input_thread;
    
    while (true)
    {
        input_thread = std::thread(getInput);
        input_thread.join();

        writeFile("", 'i');
        int command = read_commands();
        if (command & quit)
        {
            writeFile("-quit", 'o');
            break;
        }

        execute_Commands(command);
    }
    
}


#endif

#ifndef PLAY2


bool
command_found(std::ifstream& in, string& __s)
{
    getline(in, __s);
    const size_t len = __s.size();

    // Commandline is valid if its followed by '-in';
    return len > 3 and
        (__s[len - 1] == 'n') and
        (__s[len - 1] == 'i') and
        (__s[len - 2] == '-');
}

void
get_input(string& __s)
{
    const string filename = "elsa_in";

    // Break between each read
    const auto WAIT_TIME_PER_CYCLE = std::chrono::microseconds(5);

    // To read from the file
    std::ifstream infile(filename);

    // Searching for a valid commandline
    while (command_found(infile, __s) == false)
        std::this_thread::sleep_for(WAIT_TIME_PER_CYCLE);
    
    infile.close();
}


void
read_commands(const string& __s)
{
    std::vector<string> args = split(__s, ' ');
    args.pop_back();




}





void
play(vector<string>& args)
{
    const string fen = args.size() > 1 ? args[1] : startFen;
    
    std::thread input_thread;
    playboard board(fen);

    // Used to store the commands by user
    string __s;

    while (true)
    {
        input_thread = std::thread(get_input, std::ref(__s));
        input_thread.join();








    }

}



#endif

