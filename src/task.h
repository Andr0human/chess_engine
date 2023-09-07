

#ifndef TASK_H
#define TASK_H

#include "perf.h"
#include "single_thread.h"
#include "play.h"
#include <fstream>
#include <iomanip>  // for setprecision

void Init();
void Helper();
void SpeedTest();
void AccuracyTest();
void DirectSearch(const vector<string> &arg_list);
void NodeCount(const vector<string> &arg_list);
void DebugMoveGenerator(const vector<string> &_args);

void Task(int argc, char *argv[]);


#endif


