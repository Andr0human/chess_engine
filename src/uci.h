
#ifndef UCI_H
#define UCI_H

#include "base_utils.h"
#include <string>

// Write one line to stdout under the UCI output lock, then flush.
//
// Every UCI line MUST go through here. Two threads emit protocol output — the
// UCI loop (uciok / readyok / id) and the search worker (info / bestmove) — and
// FAST_IO() calls sync_with_stdio(0), which detaches std::cout from C stdio and
// so from the FILE lock that would otherwise have serialised them. Two threads
// then share one unsynchronised streambuf: concurrent sputn/overflow corrupts
// pptr, which shreds lines outright ("info depth readyok", "-72readyok readyok0")
// and can flush one buffer twice. A mangled `bestmove` or `readyok` is invisible
// to the engine but reads to the GUI as an unresponsive engine — a forfeit.
void uciSend(const std::string& line);

// Run the UCI command loop on stdin/stdout. Returns when "quit" is received
// or stdin is closed.
void uciLoop();

#endif
