

#ifndef ENDGAME_VALIDATION_H
#define ENDGAME_VALIDATION_H

#include <vector>
#include <string>

/**
 * @brief Dev-only harness for validating the hand-written endgame verdicts in
 * endgame.cpp (currently isTheoreticalDraw) against ground truth.
 *
 * This file implements the GENERATOR stage only (see docs/endgame-validation.md):
 * it exhaustively enumerates every legal position of a small material signature
 * and tallies what isTheoreticalDraw says about each. There is deliberately no
 * oracle yet, so it reports the heuristic's draw / non-draw split, not
 * correctness. CLI entry point: `elsa egvalidate`.
 */
void
validateEndgame(const std::vector<std::string>& args);

#endif
