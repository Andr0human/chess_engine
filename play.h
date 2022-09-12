

#ifndef PLAY_H
#define PlAY_H

#include "perf.h"
#include "single_thread.h"
#include "multi_thread.h"
#include <fstream>

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

extern std::ifstream inFile;
const std::string file_name = "elsa";

// Commandline : -p -th 2 -tm 1.23 0.67 -m 5069 -e

#endif

