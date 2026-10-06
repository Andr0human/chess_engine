

#ifndef ENDGAME_VALIDATION_H
#define ENDGAME_VALIDATION_H

#include <vector>
#include <string>

/**
 * @brief A development tool that checks the hand-written endgame rules in
 * endgame.cpp (isTheoreticalDraw) against exact results.
 *
 * It walks every legal position of a small material and counts what
 * isTheoreticalDraw says about each. With the `oracle` option it scores each
 * verdict against the exact WDL solver (endgame_solver.h) and prints a
 * draw-vs-decided scorecard. Without it, it prints only how many positions
 * the rules call drawn. CLI entry point: `elsa egvalidate`.
 */
void
validateEndgame(const std::vector<std::string>& args);

/**
 * @brief Solve every oracle table a signature needs from scratch, timing each,
 * and compare the results with the disk cache. A tool for working on the solver
 * itself. CLI entry point: `elsa egsolve`.
 */
void
solveEndgameTables(const std::vector<std::string>& args);

/**
 * @brief Print the oracle's verdict on one position and on each of its moves,
 * building the tables its material needs. CLI entry point: `elsa egprobe`.
 */
void
probeEndgame(const std::vector<std::string>& args);

#endif
