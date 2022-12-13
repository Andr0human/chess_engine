

#ifndef PLAY_H
#define PlAY_H

#include "perf.h"
#include "single_thread.h"
// #include "multi_thread.h"
#include <fstream>


class play_board
{
    public:
    chessBoard board;
    string commandline;
    static const int occ_pos_size = 300;
    uint64_t occured_positions[occ_pos_size];
    int pos_cnt, opponent_move;

    play_board()
    {
        board = string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        pos_cnt = 0;
        opponent_move = 0;
    }

    void
    init(const string fen)
    {
        board = fen;
        pos_cnt = 0;
        opponent_move = 0;
    }

    void
    play_move(int move)
    {
        board.MakeMove(move);
        int pt = (move >> 12) & 7, cpt = (move >> 15) & 7;
        if (pt == 1 || cpt >= 1) pos_cnt = 0;                       // If pawn or a capture move, clear game history table
        occured_positions[pos_cnt++] = board.Hash_Value;            // Add Board pos. to occured table list.

        #if defined(TRANSPOSITION_TABLE_H)
            // TT.ClearTT();
            // TT.Clear_History();
        #endif
    }

    void
    generate_history_table()
    {
        #if defined(TRANSPOSITION_TABLE_H)
        // for (int i = 0; i < pos_cnt; i++)
            // TT.RecordSearch(occured_positions[i]);
        #endif
    }
};

bool readFile();
void writeFile(string message, char file);
void write_Execute_Result();


void start_game(const vector<string> &arg_list);
int read_commands();
void execute_Commands(int command);
void find_move_for_position();
void inputRun();

void getInput();
void play();


extern play_board pb;
extern std::ifstream inFile;
const std::string file_name = "elsa";

// Commandline : -p -th 2 -tm 1.23 0.67 -m 5069 -e

#endif

