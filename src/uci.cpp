
#include "uci.h"
#include "bitboard.h"
#include "movegen.h"
#include "move_utils.h"
#include "search.h"
#include "single_thread.h"
#include "tt.h"

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
    // Fall back to a wtime/btime based budget if present.
    long long sideTime = (g_board.color == WHITE) ? wtime : btime;
    long long sideInc  = (g_board.color == WHITE) ? winc  : binc;
    if (sideTime > 0)
      moveTimeSec = (double(sideTime) / 30.0 + double(sideInc)) / 1000.0;
    else
      moveTimeSec = double(DEFAULT_SEARCH_TIME);
  }

  // Run search; output goes to a discarded sink so iterative-deepening
  // table dumps don't pollute the UCI stream.
  std::ostringstream sink;
  ChessBoard searchBoard = g_board;
  search(searchBoard, maxDepth, moveTimeSec, sink, false, true);

  Move best = info.lastIterationResult().first;
  cout << "bestmove " << moveToUci(best) << endl;
}

void
handleUciNewGame()
{
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
      // Best-effort: search runs synchronously, so by the time we read
      // "stop" the search has already completed and its bestmove was
      // printed. Nothing to do here.
    }
    else if (cmd == "quit")
    {
      break;
    }
    // Silently accept: debug, setoption, register, ponderhit, etc.
  }
}
