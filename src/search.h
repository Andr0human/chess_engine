
#ifndef SEARCH_H
#define SEARCH_H

#include "move_utils.h"
#include "perf.h"
#include "types.h"
#include "varray.h"
#include "bitboard.h"
#include "movegen.h"
#include "search_utils.h"

using std::pair;
using std::make_pair;
using std::chrono::nanoseconds;

class TestPosition
{
  string fen;
  Depth depth;
  vector<Nodes> nodes;

  public:

  TestPosition(const string str, const string testType)
  {
    const vector<string> values = base_utils::Split(str, '|');
    fen = values[0].substr(0, values[0].length() - 1);
    if (testType == "accuracy") {
      depth = 0;
      const auto nodesStr = base_utils::Split(values[1], ' ');
      nodes.resize(nodesStr.size());
      std::transform(begin(nodesStr), end(nodesStr), begin(nodes),
        [] (const string& __s) { return std::stoull(__s); }
      );
    }

    if (testType == "speed") {
      depth = std::stoi(values[1]);
      nodes = vector<Nodes>{std::stoull(values[2])};
    }
  }

  bool
  test(Nodes (*BulkCountFunc)(ChessBoard&, Depth), Depth d) const
  {
    ChessBoard pos(fen);
    return BulkCountFunc(pos, d) == nodes[d - 1];
  }

  auto
  time(Nodes (*BulkCountFunc)(ChessBoard&, Depth), int runTimes) const
  {
    using namespace std::chrono;
    ChessBoard pos(fen);
    const auto start = perf::now();

    for (int i = 0; i < runTimes; i++)
      BulkCountFunc(pos, depth);

    const auto end = perf::now();
    return duration_cast<microseconds>(end - start).count();
  }

  Nodes
  nodeCount(Depth d) const
  { return nodes[d - 1]; }

  Depth
  maxDepth() const
  { return static_cast<Depth>(nodes.size()); }

  string
  getFen() const
  { return fen; }
};

class SearchData
{
  // Set the starting point for clock
  perf_clock startTime;

  // Store which side to play for search_position
  Color side;

  uint64_t nodes, qNodes;

  // Time provided to find move for current position
  nanoseconds allotedTime;

  // Time spend on searching for move in position (in secs.)
  double timeForSearch = 0;

  Varray<Move, MAX_PLY> pvLine;

  // Stores the <best_move, eval> for each depth during search.
  Varray<pair<Move, Score>, MAX_DEPTH + 1> moveEvals;

  // Stores <move, time> for each move in each iteration.
  Varray<pair<Move, uint64_t> , MAX_MOVES> moveTimes;

  string
  ReadablePvLine(ChessBoard board) const noexcept
  {
    string res;
    bool qmoves_found = false;

    for (const Move move : pvLine)
    {
      if (((move & QS_MOVE) != 0) and !qmoves_found)
      {
        res += "(";
        qmoves_found = true;
      }

      res += PrintMove(move, board) + string(" ");
      board.MakeMove(move);
    }

    if (qmoves_found)
      res[res.size() - 1] = ')';

    return res;
  }

  public:

  SearchData()
  : startTime(perf::now()) {}

  SearchData(ChessBoard& pos, double _allotedTime)
  : startTime(perf::now()), side(pos.color), nodes(0), qNodes(0),
    allotedTime(std::chrono::duration_cast<nanoseconds>(std::chrono::duration<double>(_allotedTime)))
  {
    const MoveList myMoves = GenerateMoves(pos);
    const MoveArray movesArray = myMoves.getMoves(pos);
    Move zeroMove = movesArray[0];
    moveEvals.add(make_pair(zeroMove, VALUE_ZERO));

    for (const Move move : movesArray)
      moveTimes.add(make_pair(move, VALUE_ZERO));
  }

  bool
  IsPartOfPV(const Move m) const noexcept
  {
    const Move filteredMove = filter(m);

    for (size_t i = 0; i < pvLine.size(); i++) {
      if (filter(pvLine[i]) == filteredMove)
        return true;
    }
    return false;
  }

  bool
  TimeOver() const noexcept
  {
    nanoseconds duration = perf::now() - startTime;
    return duration >= allotedTime;
  }

  double
  TimeSpent() const noexcept
  {
    nanoseconds duration = perf::now() - startTime;
    return double(duration.count()) / 1e9;
  }

  void
  AddResult(ChessBoard pos, Score eval, Move pv[])
  {
    pvLine.clear();

    for (int i = 0; i < MAX_PV_ARRAY_SIZE; i++)
    {
      if (!IsLegalMoveForPosition(pv[i], pos))
        break;
      pvLine.add(pv[i]);
      pos.MakeMove(pv[i]);
    }
    moveEvals.add(make_pair(pv[0], eval * (2 * side - 1)));
  }

  void
  SearchCompleted() noexcept
  {
    perf_time duration = perf::now() - startTime;
    timeForSearch = duration.count();
  }

  void
  AddNode() noexcept
  { nodes++; }

  void
  AddQNode() noexcept
  { qNodes++; }

  void
  ResetNodeCount() noexcept
  { nodes = 0; qNodes = 0; }

  pair<Move, Score> LastIterationResult() const noexcept
  { return moveEvals.back(); }

  uint64_t
  TotalNodes() const noexcept
  {
    return std::accumulate(
      moveTimes.begin(), moveTimes.end(), uint64_t(0),
      [](uint64_t sum, const pair<Move, uint64_t>& moveTime) {
        return sum + moveTime.second;
      }
    );
  }

  // Prints the results of last searched depth
  void
  ShowLastDepthResult(ChessBoard pos, std::ostream& writer) const noexcept
  {
    using std::setw, std::right, std::fixed, std::setprecision;

    const size_t dep = moveEvals.size() - 1;
    const Score eval = moveEvals.back().second;
    double evalConv  = double(eval) / 100.0;

    writer << " | " << setw(6) << right << fixed << setprecision(2) << TimeSpent()
           << " | " << setw(5) << right << fixed << dep
           << " | " << setw(7) << right << fixed << setprecision(2) << evalConv
           << " | " << setw(8) << right << fixed  << TotalNodes()
           << " | " << ReadablePvLine(pos) << endl;
  }

  void ShowHeader(std::ostream& writer) const noexcept
  {
    using std::setw;
    writer << " | " << setw(6) << "Time"
           << " | " << setw(5) << "Depth"
           << " | " << setw(7) << "Score"
           << " | " << setw(8) << "Nodes"
           << " | " << "PV" << "\n";
  }

  void
  SortMovesOnTime(Move bestMove)
  {
    for (size_t i = 0; i < moveTimes.size(); i++)
    {
      if (filter(bestMove) == filter(moveTimes[i].first))
      {
        std::swap(moveTimes[i], moveTimes[0]);
        break;
      }
    }

    std::sort(moveTimes.begin() + 1, moveTimes.end(), [](const auto &a, const auto &b) {
      return a.second > b.second;
    });
  }

  void Print(ChessBoard pos)
  {
    int moveNo = 1;
    for (const auto& [move, tm] : moveTimes)
      cout << moveNo++ << "\t| " << PrintMove(move, pos) << "\t| " << tm << endl;
  }

  void
  InsertMoveToList(size_t moveNo)
  {
    moveTimes[moveNo].second = (2 * nodes + qNodes) >> 1;
    ResetNodeCount();
  }

  MoveArray
  getMoves () const
  {
    MoveArray movesArray;

    for (const auto& moveTime : moveTimes)
      movesArray.add(moveTime.first);

    return movesArray;
  }
};

void OrderMoves(const ChessBoard& pos, MoveArray& movesArray, bool pv_moves, bool check_moves);

Score
SeeScore(const ChessBoard& pos, Move move);

/**
 * @brief Prints all encoded-moves in list to human-readable strings
 *
 * @param myMoves Movelist for board positions.
 * @param pos board position
 */
void
PrintMovelist(MoveArray myMoves, ChessBoard pos);

extern SearchData info;

#endif
