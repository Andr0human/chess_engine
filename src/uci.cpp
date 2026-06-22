
#include "uci.h"
#include "bitboard.h"
#include "movegen.h"
#include "move_utils.h"
#include "search.h"
#include "single_thread.h"
#include "tt.h"

#include <algorithm>
#include <atomic>
#include <iostream>
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

// Persistent board across commands within a session.
ChessBoard g_board(START_FEN);

// The search runs on this worker so the UCI loop stays responsive to
// stop/quit/isready while thinking. Only one search runs at a time (it owns
// the global `info` and the shared TT), so callers stop-and-join the previous
// worker before starting anything that touches that shared state.
std::thread g_worker;

// Raise the abort flag (polled by the search via SearchData::shouldStop) and
// wait for the worker to unwind and emit its `bestmove`. Safe to call when no
// search is running. `searchStop` is left set; handleGo clears it before the
// next launch.
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
  cout << "id name Elsa" << endl;
  cout << "id author Andr0human" << endl;
  cout << "uciok" << endl;
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
    // Read FEN fields until we hit "moves" or the stream ends. FEN is
    // typically six space-separated fields, but we don't rely on the
    // count — we stop on the "moves" sentinel so we don't accidentally
    // swallow it (or stop one token short of it).
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

// Decide how long to search given the side-to-move's remaining clock and
// increment (both in milliseconds). Faithful 1:1 port of the heuristic that
// used to live on the Unity side (ChessEngine.DecideTimeForSearch): the GUI
// now forwards the raw clock and the engine owns the time-management decision
// — the correct UCI split. Returns the budget in seconds.
double
decideSearchTime(long long sideTimeMs, long long sideIncMs)
{
  const double timeLeft  = double(sideTimeMs) / 1000.0;  // seconds
  const double increment = double(sideIncMs)  / 1000.0;  // seconds

  // Estimate moves remaining from how much material is left: a full board
  // (weight 7880) implies ~32 moves to go; as material comes off the estimate
  // shrinks, so each remaining move gets a larger slice. Material weights
  // match Unity's PositionWeight(): P100 N320 B300 R500 Q900.
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

  // Floor at 1 ms so the search always gets a sane positive budget
  // (matches the Mathf.Max(1, ...) the Unity send site applied).
  return std::max(searchTime, 0.001);
}

void
handleGo(stringstream& ss)
{
  // Parse a subset of UCI go: movetime, wtime/btime/winc/binc, depth, infinite.
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
      moveTimeSec = 1e9;
    }
    // Unknown tokens are ignored silently.
  }

  if (moveTimeSec < 0)
  {
    // No explicit movetime: derive a search budget from the side-to-move's
    // clock (or fall back to the default if no clock was provided).
    long long sideTime = (g_board.color == WHITE) ? wtime : btime;
    long long sideInc  = (g_board.color == WHITE) ? winc  : binc;

    moveTimeSec = (sideTime > 0)
        ? decideSearchTime(sideTime, sideInc)
        : double(DEFAULT_SEARCH_TIME);
  }

  // Stop any prior search and launch this one on the worker. The board is
  // copied into the lambda so later `position` edits can't disturb a running
  // search. The worker prints `bestmove` when search() returns — whether it
  // ended by depth/time or an async `stop`. Iterative-deepening table dumps go
  // to a discarded sink; only the UCI `info`/`bestmove` lines reach stdout.
  stopAndJoin();
  searchStop.store(false, std::memory_order_relaxed);

  g_worker = std::thread([board = g_board, maxDepth, moveTimeSec]() {
    std::ostringstream sink;
    search(board, maxDepth, moveTimeSec, sink, false, true);
    cout << "bestmove " << moveToUci(info.lastIterationResult().first) << endl;
  });
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
      cout << "readyok" << endl;
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
      // Raise the abort flag; the worker observes it at its next checkpoint,
      // unwinds, and prints `bestmove`. It is joined on the next go/quit.
      searchStop.store(true, std::memory_order_relaxed);
    }
    else if (cmd == "quit")
    {
      stopAndJoin();
      break;
    }
    // Silently accept: debug, setoption, register, ponderhit, etc.
  }

  // Reached on EOF (stdin closed) without an explicit `quit`: never let a
  // joinable std::thread destruct, which would std::terminate the process.
  stopAndJoin();
}
