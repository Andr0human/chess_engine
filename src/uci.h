
#ifndef UCI_H
#define UCI_H

#include "base_utils.h"

// Run the UCI command loop on stdin/stdout. Returns when "quit" is received
// or stdin is closed.
void uciLoop();

#endif
