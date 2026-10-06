
#ifndef TUNER_H
#define TUNER_H

#include <vector>
#include <string>

// Texel-style eval weight tuner.
//
//   elsa tune [data <path> | --all [dir <folder>]] [iters <n>] [weights <list>]
//
// With no `data` path it only runs the self-check (the eval rebuilt from cached
// terms must equal the real static eval). With a labelled dataset it fits the
// sigmoid constant K, then runs coordinate descent on EvalWeights to minimise the
// mean squared error. It prints the MSE before and after, and the tuned weights.
// It doesn't change the engine's defaults; paste the printed values in by hand.
// `weights` is a comma-separated list of the weights to tune; the rest stay
// fixed.
//
//   elsa tune pst [data <path> | --all [dir <folder>]] [tables <list>] [unfold <list>]
//                 [iters <n>] [free]
//
// Tunes piece-square tables by gradient descent, holding the weights and every other
// table fixed, and prints them as C++ tables. `tables` is a comma-separated list such as
// queenMg,queenEg, where mg and eg mean all six of that phase; the default is eg.
// Tables are kept left-right symmetric unless `unfold` lists them (same syntax).
// `free` lets each table's mean drift (material).
void
tuneEval(const std::vector<std::string>& args);

#endif
