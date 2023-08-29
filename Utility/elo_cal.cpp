#include <cstdio>
#include <cstdlib>
#include <cmath>

int main(int argc, char **argv) {
  if (argc != 4) {
    std::printf("Wrong number of arguments.\n\nUsage:%s <wins> <losses> <draws>\n", argv[0]);
    return 1;
  }
  int wins = std::atoi(argv[1]);
  int losses = std::atoi(argv[2]);
  int draws = std::atoi(argv[3]);

  double games = wins + losses + draws;
  std::printf("Number of games: %g\n", games);
  double winning_fraction = (wins + 0.5*draws) / games;
  std::printf("Winning fraction: %g\n", winning_fraction);
  double elo_difference = -std::log(1.0/winning_fraction-1.0)*400.0/std::log(10.0);
  std::printf("Elo difference: %+g\n", elo_difference);
  double los = .5 + .5 * std::erf((wins-losses)/std::sqrt(2.0*(wins+losses)));
  std::printf("LOS: %g\n", los);
}

