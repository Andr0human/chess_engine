

#ifndef TASK_H
#define TASK_H

#include "perf.h"
#include "single_thread.h"
// #include "multi_thread.h"
#include "play.h"
// #include "ponder.h"
#include <fstream>
#include <iomanip>  // for setprecision

void init();
void helper();
void speed_test();
void accuracy_test();
void direct_search(const vector<string> &arg_list);
void node_count(const vector<string> &arg_list);
void debug_movegen(const vector<string> &_args);

void task(int argc, char *argv[]);


#endif


