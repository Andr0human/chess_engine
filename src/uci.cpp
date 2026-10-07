
#include "uci.h"
#include "bitboard.h"
#include "lookup_table.h"
#include "movegen.h"
#include "move_utils.h"
#include "search.h"
#include "single_thread.h"
#include "tt.h"

#include <algorithm>
#include <atomic>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::stringstream;

namespace
{

// Guards every write to stdout. uciSend() in uci.h explains why it's needed.
std::mutex g_outMutex;

// The board, kept across commands for the whole session.
ChessBoard g_board(START_FEN);

// The search runs on this thread so the UCI loop can still answer stop, quit
// and isready while it thinks. Only one search runs at a time, since it uses
// the global `info` and the TT. Stop and join the old worker before starting
// anything that touches them.
std::thread g_worker;

// Set the stop flag (the search checks it in SearchData::shouldStop) and wait
// for the worker to finish and print `bestmove`. Safe to call when no search
// is running. searchStop stays set; handleGo clears it before the next search.
void
stopAndJoin()
{
  if (g_worker.joinable())
  {
    searchStop.store(true, std::memory_order_relaxed);
    g_worker.join();
  }
}

void
sendId()
{
  uciSend("id name " + ENGINE_NAME + " " + ENGINE_VERSION + " " + plt::SLIDER_INDEXING);
  uciSend("id author Andr0human");
  uciSend("option name Hash type spin default " + std::to_string(int(TT_DEFAULT_MB))
          + " min " + std::to_string(int(TT_MIN_MB))
          + " max " + std::to_string(int(TT_MAX_MB)));
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
    // Optional "moves" follows.
    if (ss >> token && token == "moves")
      sawMoves = true;
  }
  else if (token == "fen")
  {
    // Read FEN fields until "moves" or the end of the line. A FEN usually has
    // six fields, but don't count on it. Stop at "moves" so it isn't read as
    // part of the FEN.
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

// Time kept back for sending the move and for process latency, so the engine
// never plans to think right up to the flag. In-process (Chessmate) the
// latency is almost zero, but over an external GUI's pipes (cutechess,
// fastchess) it is real. 40 ms is above normal pipe latency and costs little
// thinking time at blitz. Raise it if games are lost on time with an external
// GUI.
constexpr double MOVE_OVERHEAD = 0.040;  // seconds

// Decide how long to search, given the side to move's remaining clock and
// increment (both in milliseconds). Returns the budget in seconds.
double
decideSearchTime(long long sideTimeMs, long long sideIncMs)
{
  // Take the overhead off the clock first, so the budget formula and the 62%
  // cap below only plan with time that can really be spent.
  const double timeLeft  =
      std::max(0.0, double(sideTimeMs) / 1000.0 - MOVE_OVERHEAD);  // seconds
  const double increment = double(sideIncMs)  / 1000.0;            // seconds

  // Estimate the moves left from the material left. A full board (weight 7880)
  // means about 32 moves to go. The estimate shrinks as material comes off, so
  // each move gets a bigger share. The piece values match Unity's
  // PositionWeight(): P100 N320 B300 R500 Q900.
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

  double searchTime = ((timeLeft + increment) / movesToGo) + (0.9 * increment);

  // Never spend more than 62% of the remaining clock on one move.
  searchTime = std::min(searchTime, 0.62 * timeLeft);

  // At least 1 ms, so the search always gets a positive budget (the same floor
  // as Unity's Mathf.Max(1, ...)).
  return std::max(searchTime, 0.001);
}

// Means "no time limit": so many seconds that the clock never ends the
// search, and only `stop` or the depth limit does. Used for `go infinite` and
// for a `go` with no time limit. It's a finite number so the duration_cast in
// SearchData stays well defined. 1e9 s is about 1e18 ns, which fits in int64.
constexpr double NO_TIME_LIMIT = 1e9;  // seconds

void
handleGo(stringstream& ss)
{
  // Parse a subset of UCI go: movetime, wtime/btime/winc/binc, depth, infinite.
  double moveTimeSec = -1.0;
  Depth maxDepth = MAX_DEPTH;

  long long wtime = -1, btime = -1, winc = 0, binc = 0;
  bool sawClock = false;

  string token;
  while (ss >> token)
  {
    if (token == "movetime")
    {
      long long ms = 0;
      ss >> ms;
      moveTimeSec = double(ms) / 1000.0;
    }
    else if (token == "wtime") { ss >> wtime; sawClock = true; }
    else if (token == "btime") { ss >> btime; sawClock = true; }
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
      moveTimeSec = NO_TIME_LIMIT;
    }
    // Unknown tokens are ignored.
  }

  if (moveTimeSec < 0)
  {
    // No movetime. Only use a time budget if the GUI gave a time limit. With a
    // clock the engine manages its own time. `go depth <n>` and a bare `go`
    // give no time, so they run until the depth limit or `stop`. A default
    // cap would stop them early: En Croissant's analysis sends `go depth 20`
    // and later `stop`, and would get a shallower search than it asked for.
    long long sideTime = (g_board.color == WHITE) ? wtime : btime;
    long long sideInc  = (g_board.color == WHITE) ? winc  : binc;

    if (sideTime > 0)
      moveTimeSec = decideSearchTime(sideTime, sideInc);
    else if (sawClock)
      // A clock was sent but the side to move has no time left (flagged, or a
      // bad value). Search for the default time instead of thinking forever.
      moveTimeSec = double(DEFAULT_SEARCH_TIME);
    else
      moveTimeSec = NO_TIME_LIMIT;
  }

  // Stop any running search and start this one on the worker. The board is
  // copied into the lambda so a later `position` can't change a running
  // search. The worker prints `bestmove` when search() returns, whether it hit
  // the depth or time limit or got `stop`. The per-depth table goes to a sink
  // that is thrown away, so only `info` and `bestmove` lines reach stdout.
  stopAndJoin();
  searchStop.store(false, std::memory_order_relaxed);

  g_worker = std::thread([board = g_board, maxDepth, moveTimeSec]() {
    std::ostringstream sink;
    search(board, maxDepth, moveTimeSec, sink, false, true);
    uciSend("bestmove " + moveToUci(info.bestMoveFound()));
  });
}

// `setoption name <id> [value <x>]`. Hash is the only option advertised. Any
// other option is accepted and ignored.
void
handleSetOption(stringstream& ss)
{
  string token, name;
  if (!(ss >> token) or token != "name")
    return;

  // The UCI grammar lets an option name contain spaces, so the name is
  // everything between `name` and the `value` keyword.
  while (ss >> token and token != "value")
    name += (name.empty() ? "" : " ") + token;

  if (name != "Hash" or token != "value")
    return;

  // Read as a number instead of with stoul, so bad text from the GUI can't
  // throw on a thread with no handler. A bad value leaves the table as it was.
  unsigned long long mb = 0;
  if (!(ss >> mb))
    return;

  // resize() frees and reallocates both tables, so it can't run during a
  // search (ucinewgame stops first for the same reason). All stored entries
  // are lost, which is expected.
  stopAndJoin();
  if constexpr (USE_TT) {
    tt.resize(size_t(mb));
  }
}

void
handleUciNewGame()
{
  // Stop first: clearing the TT under a live search would race the worker.
  stopAndJoin();
  g_board = ChessBoard(START_FEN);
  if constexpr (USE_TT) {
    tt.clear();
  }
}

} // namespace

void
uciSend(const string& line)
{
  std::lock_guard<std::mutex> lock(g_outMutex);
  cout << line << endl;
}

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
      handleUciNewGame();
    }
    else if (cmd == "position")
    {
      handlePosition(ss);
    }
    else if (cmd == "go")
    {
      handleGo(ss);
    }
    else if (cmd == "stop")
    {
      // Set the stop flag. The worker sees it at its next check, returns and
      // prints `bestmove`. It is joined on the next go or quit.
      searchStop.store(true, std::memory_order_relaxed);
    }
    else if (cmd == "setoption")
    {
      handleSetOption(ss);
    }
    else if (cmd == "quit")
    {
      stopAndJoin();
      break;
    }
    // Ignore the rest: debug, register, ponderhit, etc.
  }

  // Reached when stdin closes without `quit`. Destroying a joinable
  // std::thread would terminate the process, so join it first.
  stopAndJoin();
}
