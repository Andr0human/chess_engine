

#ifndef PONDER_H
#define PONDER_H

#include <fstream>
#include <thread>
#include <vector>
#include "single_thread.h"
#include "multi_thread.h"

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

#endif


