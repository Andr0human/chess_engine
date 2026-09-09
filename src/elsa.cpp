#include "task.h"
#include "uci.h"


int main(int argc, char **argv)
{
  FAST_IO();

  const auto args = utils::extractArgumentList(argc, argv);
  init(args);

  // No args is how UCI GUIs launch us; also enter the UCI loop if "uci" was
  // passed explicitly.
  if (args.empty() || (!args.empty() && args.front() == "uci"))
  {
    uciLoop();
    return 0;
  }

  task(args);
}
