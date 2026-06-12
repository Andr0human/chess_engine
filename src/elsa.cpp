#include "task.h"
#include "uci.h"


int main(int argc, char **argv)
{
  FAST_IO();

  const auto args = utils::extractArgumentList(argc, argv);
  init(args);

  // Default to UCI when no command is given (how UCI GUIs launch us)
  // or when "uci" is passed explicitly on the command line.
  if (args.empty() || (!args.empty() && args.front() == "uci"))
  {
    uciLoop();
    return 0;
  }

  task(args);
}
