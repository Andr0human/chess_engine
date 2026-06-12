#include "task.h"
#include "uci.h"


int main(int argc, char **argv)
{
  FAST_IO();

  // const ChessBoard pos("1b2k3/8/PK6/8/8/8/8/8 w - - 0 1");
  // const auto res = isTheoreticalDraw(pos);
  // std::cout << "is draw = " << res << std::endl;

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
