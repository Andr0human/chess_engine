

#ifndef PLAY_H
#define PLAY_H

#include "perf.h"
#include "single_thread.h"
#include <fstream>
#include <thread>

using std::ifstream;
using std::ofstream;


/*** CommandList
 * go - To start searching for the best move

 * position - set the current position using fen
 * position [fen <FEN string> | startpos]

 * moves - play the current in move(s) in set position. (Do a legality-check first)
 * moves <move1> <move2> ... <movei>

 * time - time allocated for the search in secs. [default : 2 sec.]

 * input - set input file for chess_engine

 * output - set output file for chess_engine

 * threads - threads used for the search

 * depth - maxDepth to be searched [default : 36] 

 * quit - exits the program

 * Sample:
   go position <fen> moves <move1> <move2> ... <movei> movetime 0.334 threads 4 depth 16 quit
***/


class PlayBoard : public ChessBoard
{
  private:
  // Max threas to use for search
  size_t threads;

  // Max depth to search
  size_t mDepth;
  double movetime;
  bool goReceive;
  bool toQuit;

  int queryNumber;

  // Moves to be played on PlayBoard before starting Search
  vector<Move    > preMoves;
  vector<uint64_t> prevKeys;

  bool
  isNumeric(const std::string& s)
  {
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
  }

  public:
  // Init
  PlayBoard() :
  threads(1), mDepth(MAX_DEPTH), movetime(DEFAULT_SEARCH_TIME),
  goReceive(false), toQuit(false), queryNumber(1) {}

  void
  setThreads(size_t __tc) noexcept
  { threads = __tc; }

  void
  setDepth(size_t __dep) noexcept
  { mDepth = __dep; }

  void
  setMoveTime(double __mt) noexcept
  { movetime = __mt; }

  double
  getMoveTime() const noexcept
  { return movetime; }

  void
  readySearch() noexcept
  { goReceive = true; }

  void
  searchDone() noexcept
  { goReceive = false; }

  bool
  doSearch() const noexcept
  { return goReceive; }

  void
  readyQuit() noexcept
  { toQuit = true; }

  bool
  doQuit() const noexcept
  { return toQuit; }

  void
  setNewPosition(const string& fen) noexcept
  {
    prevKeys.clear();
    setPositionWithFen(fen);
  }

  void
  addPremoves(const vector<string>& list, size_t index) noexcept
  {
    while (index < list.size() and isNumeric(list[index]))
    {
      int move = std::stoi( list[index++] );
      preMoves.push_back(move);
    }
  }

  void
  addPremoves(const Move move)
  {
    preMoves.push_back(move);
    playPreMoves();
  }

  bool
  preMovesExist() const noexcept
  { return !preMoves.empty(); }

  void
  playPreMoves() noexcept
  {
    const auto pawnMove = [] (Move move)
    { return ((move >> 12) & 7) == 1; };

    const auto captures = [] (Move move)
    { return ((move >> 15) & 7) > 0; };

    for (const Move move : preMoves)
    {
      if (pawnMove(move) or captures(move))
        prevKeys.clear();

      makeMove(move, false);
      prevKeys.push_back(hashValue);
    }

    preMoves.clear();
  }

  vector<uint64_t>
  getPreviousHashkeys() const noexcept
  { return prevKeys; }

  bool
  integrityCheck() const noexcept
  {
    if (prevKeys.empty())
      return true;

    uint64_t lastKey = prevKeys.back();
    return generateHashkey() == lastKey;
  }

  inline int
  query() const noexcept
  { return queryNumber; }

  inline void
  updateQuery() noexcept
  { queryNumber++; }
};


void
play(const vector<string>& args);


#endif

