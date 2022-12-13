

#ifndef PONDER_H
#define PONDER_H

#include <fstream>
#include <thread>
#include <vector>
#include "single_thread.h"
// #include "multi_thread.h"


class ponder_list
{
    void index_swap(uint64_t i, uint64_t j);

    public:
    int moves[maxMoves];
    int evals[maxMoves];
    int lines[maxMoves][maxDepth];
    uint64_t mCount;

    ponder_list() {mCount = 0;};

    void
    setList(MoveList &myMoves);

    void
    insert(int idx, int move, int eval, int line[]);

    void
    show(chessBoard &board);

    void
    sortlist();
};


bool read_input();
bool read_stop_input();
void input_run();
void input_stop_run();
void write_file(std::string msg, char file_type);
void write_search_result();


void ponder();
bool extract_commandline(chessBoard &_cb);
void execute_commandline(chessBoard &_cb);

void ponderSearch(chessBoard board, bool file_print = false);


extern ponder_list pdl;

#endif


