#include "task.h"
#include "uci.h"


int main(int argc, char **argv)
{
  FAST_IO();

  const auto args = utils::extractArgumentList(argc, argv);
  init(args);

  // UCI GUIs start the engine with no arguments. Also run the UCI loop if
  // "uci" is passed.
  if (args.empty() || (!args.empty() && args.front() == "uci"))
  {
    uciLoop();
    return 0;
  }

  task(args);
}
