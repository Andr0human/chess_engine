
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

// Serialises every write to stdout. See uciSend() in uci.h for why std::cout
// has no lock of its own here.
std::mutex g_outMutex;

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
  uciSend("id name " + ENGINE_NAME + " " + ENGINE_VERSION);
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

// A fixed slice of the clock reserved for move-transmission / process latency
// so we never plan to think right up to the flag. In-process (Chessmate) this
// is nearly free, but over an external GUI's stdio pipes (cutechess/fastchess)
// the go→bestmove round-trip latency is real and un-budgeted; a self-play
// sanity run flagged once on time even in-process (zero margin). 40 ms sits
// comfortably above typical pipe latency without eating meaningfully into
// think time at blitz. Tune up if time-losses ever appear over an external GUI.
constexpr double MOVE_OVERHEAD = 0.040;  // seconds

// Decide how long to search given the side-to-move's remaining clock and
// increment (both in milliseconds). Faithful 1:1 port of the heuristic that
// used to live on the Unity side (ChessEngine.DecideTimeForSearch): the GUI
// now forwards the raw clock and the engine owns the time-management decision
// — the correct UCI split. Returns the budget in seconds.
double
decideSearchTime(long long sideTimeMs, long long sideIncMs)
{
  // Shave the overhead off the usable clock up front so both the budget formula
  // and the 62% cap below plan against time we can actually afford to spend.
  const double timeLeft  =
      std::max(0.0, double(sideTimeMs) / 1000.0 - MOVE_OVERHEAD);  // seconds
  const double increment = double(sideIncMs)  / 1000.0;            // seconds

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

// Stand-in for "no time limit": a budget so large the clock can never end the
// search, leaving `stop` (or the depth limit) as the only terminator. Used for
// `go infinite` and for any `go` that names no time constraint at all. Kept as
// a finite number of seconds rather than an infinity so the duration_cast in
// SearchData stays well-defined; 1e9 s is ~1e18 ns, comfortably inside int64.
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
    // Unknown tokens are ignored silently.
  }

  if (moveTimeSec < 0)
  {
    // No explicit movetime. A time budget is only appropriate when the GUI
    // actually gave us a time constraint: with a clock we manage it ourselves,
    // but `go depth <n>` (and a bare `go`) name no time at all and must run
    // until the depth limit or an async `stop` — capping those at a default
    // makes the GUI's setting silently inert. En Croissant's analysis pane
    // sends exactly these forms (`go depth 20` ... `stop`), and under the old
    // 1s fallback a 20-ply request returned at depth 12.
    long long sideTime = (g_board.color == WHITE) ? wtime : btime;
    long long sideInc  = (g_board.color == WHITE) ? winc  : binc;

    if (sideTime > 0)
      moveTimeSec = decideSearchTime(sideTime, sideInc);
    else if (sawClock)
      // A clock was sent but the side to move has none left (flagged, or a
      // malformed value). Nothing to manage; answer with the floor rather than
      // thinking forever on a lost clock.
      moveTimeSec = double(DEFAULT_SEARCH_TIME);
    else
      moveTimeSec = NO_TIME_LIMIT;
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
    uciSend("bestmove " + moveToUci(info.lastIterationResult().first));
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
