
#ifndef UCI_H
#define UCI_H

#include "base_utils.h"
#include <string>

// Write one line to stdout under the UCI output lock, then flush.
//
// Every UCI line must go through here. Two threads write protocol output: the
// UCI loop (uciok, readyok, id) and the search worker (info, bestmove).
// FAST_IO() calls sync_with_stdio(0), which unlinks std::cout from C stdio and
// from the lock that came with it. Without this lock, writes from the two
// threads get mixed into broken lines (like "info depth readyok"). A GUI that
// gets a broken bestmove or readyok treats the engine as unresponsive.
void uciSend(const std::string& line);

// Run the UCI command loop on stdin/stdout. Returns when "quit" is received
// or stdin is closed.
void uciLoop();

#endif
