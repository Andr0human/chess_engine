#ifndef UCI_H
#define UCI_H

// Minimal UCI front-end backported onto the 1.0.0 engine so it can be driven
// by an external match runner (cutechess-cli / fastchess / Arena) and played
// head-to-head against later releases. 1.0.0 shipped no UCI: it talked to
// Chessmate through the bespoke `play` interface in play.h, which no GUI
// speaks. Nothing below the protocol layer is modified -- search, evaluation
// and move generation are exactly as tagged -- so a match against this binary
// measures 1.0.0's playing strength and not a rewritten engine.
//
// This branch exists only to produce that opponent binary. Do not merge it.

void
uciLoop();

#endif
