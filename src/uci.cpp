
#include "uci.h"
#include "bitboard.h"
#include "movegen.h"
#include "move_utils.h"
#include "search.h"
#include "single_thread.h"
#include "tt.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::stringstream;

namespace
{

// Persistent board across commands within a session.
ChessBoard g_board(START_FEN);

// 1.0.0's search() is a blocking call with no abort flag, so this loop runs the
// search inline on the reader thread rather than on a worker. Consequences,
// both acceptable for match play: `stop` cannot interrupt a running search (it
// is accepted and ignored), and no command is read until `bestmove` is out.
// cutechess-cli and fastchess only send `stop` when pondering or when an engine
// has already burned its clock, neither of which happens in the fixed-movetime
// matches this binary exists for.
void
uciSend(const string& line)
{
  cout << line << endl;
}

void
sendId()
{
  uciSend("id name Elsa 1.0.0");
  uciSend("id author Andr0human");
  uciSend("uciok");
}

void
handlePosition(stringstream& ss)
{
  string token;
  if (!(ss >> token))
    return;

  string fen;
  bool sawMoves = false;

  if (token == "startpos")
  {
    fen = START_FEN;
    if (ss >> token && token == "moves")
      sawMoves = true;
  }
  else if (token == "fen")
  {
    // Read FEN fields until "moves" or end of line. We stop on the sentinel
    // rather than counting six fields, so a short or long FEN still parses.
    fen.clear();
    while (ss >> token)
    {
      if (token == "moves") { sawMoves = true; break; }
      if (!fen.empty()) fen += ' ';
      fen += token;
    }
  }
  else
  {
    return;
  }

  g_board = ChessBoard(fen);

  if (sawMoves)
  {
    string mv;
    while (ss >> mv)
    {
      Move m = moveFromUci(mv, g_board);
      if (m == NULL_MOVE)
        break;
      g_board.makeMove(m, false);
    }
  }
}

// A fixed slice of the clock held back for move-transmission latency, so we
// never plan to think right up to the flag.
constexpr double MOVE_OVERHEAD = 0.040;  // seconds

// Decide how long to search from the side-to-move's remaining clock and
// increment (both milliseconds). 1.0.0 had no time management of its own --
// Chessmate's Unity side computed the budget and handed it over as a movetime
// -- so this is the same heuristic later releases adopted into the engine.
// Keeping the two identical is what makes a clock-based match a comparison of
// search and evaluation rather than of time management. Returns seconds.
double
decideSearchTime(long long sideTimeMs, long long sideIncMs)
{
  const double timeLeft  =
      std::max(0.0, double(sideTimeMs) / 1000.0 - MOVE_OVERHEAD);
  const double increment = double(sideIncMs) / 1000.0;

  // Estimate moves remaining from material left on the board: a full board
  // implies ~32 moves to go, and the estimate shrinks as pieces come off.
  const double maxMoves  = 32.0;
  const double maxWeight = 7880.0;
  const double currentWeight =
      100.0 * g_board.count<PAWN>()   +
      320.0 * g_board.count<KNIGHT>() +
      300.0 * g_board.count<BISHOP>() +
      500.0 * g_board.count<ROOK>()   +
      900.0 * g_board.count<QUEEN>();

  const double movesToGo =
      maxMoves - (((maxWeight - currentWeight) / 400.0) * 1.3);

  double searchTime = ((timeLeft + increment) / movesToGo) + (0.6 * increment);

  // Never spend more than 62% of the remaining clock on one move.
  searchTime = std::min(searchTime, 0.62 * timeLeft);

  return std::max(searchTime, 0.001);
}

void
handleGo(stringstream& ss)
{
  double moveTimeSec = -1.0;
  Depth maxDepth = MAX_DEPTH;

  long long wtime = -1, btime = -1, winc = 0, binc = 0;

  string token;
  while (ss >> token)
  {
    if (token == "movetime")
    {
      long long ms = 0;
      ss >> ms;
      moveTimeSec = double(ms) / 1000.0;
    }
    else if (token == "wtime") { ss >> wtime; }
    else if (token == "btime") { ss >> btime; }
    else if (token == "winc")  { ss >> winc;  }
    else if (token == "binc")  { ss >> binc;  }
    else if (token == "depth")
    {
      int d = 0;
      ss >> d;
      maxDepth = Depth(d);
    }
    else if (token == "infinite")
    {
      // No abort flag to end it, so treat this as "search a long time".
      moveTimeSec = 1e9;
    }
    // Unknown tokens are ignored silently.
  }

  if (moveTimeSec < 0)
  {
    long long sideTime = (g_board.color == WHITE) ? wtime : btime;
    long long sideInc  = (g_board.color == WHITE) ? winc  : binc;

    moveTimeSec = (sideTime > 0)
        ? decideSearchTime(sideTime, sideInc)
        : double(DEFAULT_SEARCH_TIME);
  }

  // search() bails out early on a position with no legal moves, leaving `info`
  // holding the *previous* search's result -- which would be reported here as a
  // legal move in an already-finished game. Answer the null move instead.
  if (generateMoves(g_board).countMoves() == 0)
  {
    uciSend("bestmove 0000");
    return;
  }

  // The iterative-deepening table goes to a discarded sink: only `bestmove`
  // reaches stdout. 1.0.0 emits no UCI `info` lines, so match logs will show no
  // depth or score for this engine -- expected, not a fault.
  std::ostringstream sink;
  search(g_board, maxDepth, moveTimeSec, sink, false);

  uciSend("bestmove " + moveToUci(info.lastIterationResult().first));
}

} // namespace

void
uciLoop()
{
  string line;
  while (std::getline(cin, line))
  {
    stringstream ss(line);
    string cmd;
    if (!(ss >> cmd))
      continue;

    if (cmd == "uci")
    {
      sendId();
    }
    else if (cmd == "isready")
    {
      uciSend("readyok");
    }
    else if (cmd == "ucinewgame")
    {
      g_board = ChessBoard(START_FEN);
      if constexpr (USE_TT) {
        tt.clear();
      }
    }
    else if (cmd == "position")
    {
      handlePosition(ss);
    }
    else if (cmd == "go")
    {
      handleGo(ss);
    }
    else if (cmd == "quit")
    {
      break;
    }
    // Silently accept: stop, debug, setoption, register, ponderhit, etc.
  }
}
