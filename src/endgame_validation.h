

#ifndef ENDGAME_VALIDATION_H
#define ENDGAME_VALIDATION_H

#include <vector>
#include <string>

/**
 * @brief Dev-only harness for validating the hand-written endgame verdicts in
 * endgame.cpp (currently isTheoreticalDraw) against ground truth.
 *
 * It exhaustively enumerates every legal position of a small material signature
 * and tallies what isTheoreticalDraw says about each. With the `oracle` option it
 * scores each verdict against the perfect WDL solver (endgame_solver.h) and prints
 * the draw-vs-decided confusion matrix; without it, only the heuristic's draw /
 * non-draw split. CLI entry point: `elsa egvalidate`.
 */
void
validateEndgame(const std::vector<std::string>& args);

#endif
