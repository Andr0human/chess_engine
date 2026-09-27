
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
//   elsa tune pst [data <path> | --all [dir <folder>]] [tables <list>] [iters <n>] [free]
//
// Tunes piece-square tables by gradient descent, holding the weights and every other
// table fixed, and prints them as C++ tables. `tables` is a comma-separated list such as
// queenMg,queenEg, where mg and eg mean all six of that phase; the default is eg.
// `free` lets each table's mean drift (material).
void
tuneEval(const std::vector<std::string>& args);

#endif
