
#ifndef TUNER_H
#define TUNER_H

#include <vector>
#include <string>

// Texel-style evaluation weight tuner.
//
//   elsa tune [data <path>] [iters <n>]
//
// With no `data` path it only runs the correctness self-check (the reconstruction
// from cached components must reproduce the real white-relative static eval). With a
// labeled dataset it fits the sigmoid scaling constant K, then coordinate-descends the
// runtime EvalWeights to minimise mean-squared prediction error, printing the MSE
// before/after and the tuned weights. It does NOT overwrite the engine's defaults —
// the tuned values are printed for the user to paste in deliberately.
//
//   elsa tune pst [data <path> | --all [dir <folder>]] [iters <n>] [free]
//
// Tunes the endgame piece-square tables by gradient descent, holding the weights fixed,
// and prints them as C++ tables. `free` lets each table's mean drift (material).
void
tuneEval(const std::vector<std::string>& args);

#endif
