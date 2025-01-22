
#ifndef SEARCH_H
#define SEARCH_H

#include "perf.h"
#include "bitboard.h"
#include "movegen.h"
#include "search_utils.h"

using std::pair;
using std::make_pair;
using std::to_string;
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

  // Stores length of pv-line.
  Ply pvLength;

  // Time provided to find move for current position
  nanoseconds allotedTime;

  // Time spend on searching for move in position (in secs.)
  double timeForSearch = 0;

  // Stores the pv of the lastest search
  Move pvLine[MAX_PLY];

  // Stores current depth
  Depth depth;

  // Stores the <best_move, eval> for each depth during search.
  vector<pair<Move, Score>> moveEvals;

  // Stores <move, time> for each move in each iteration.
  pair<Move, uint64_t> moveTimes[MAX_MOVES];

  size_t moveCount = 0;

  string
  ReadablePvLine(ChessBoard board) const noexcept
  {
    string res;
    bool qmoves_found = false;

    for (Ply i = 0; i < pvLength; i++)
    {
      const Move move = pvLine[i];
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
  : startTime(perf::now()), pvLength(0) {}

  SearchData(ChessBoard& position, double _allotedTime)
  : startTime(perf::now()), side(position.color), pvLength(0),
    allotedTime(std::chrono::duration_cast<nanoseconds>(std::chrono::duration<double>(_allotedTime)))
  {
    const MoveList myMoves = GenerateMoves(position);
    Move zeroMove = myMoves.pMoves[0];
    moveEvals.emplace_back(std::make_pair( zeroMove, 0 ));
    
    std::transform(myMoves.begin(), myMoves.end(), moveTimes,
        [] (const Move m) { return make_pair(m, 0); }
    );
    moveCount = myMoves.size();
  }

  bool
  IsPartOfPV(const Move m) const noexcept
  {
    const Move filteredMove = filter(m);

    for (Ply i = 0; i < pvLength; i++) {
      if (filter(pvLine[i]) == filteredMove)
        return true;
    }
    return false;
  }

  bool
  TimeOver() noexcept
  {
    nanoseconds duration = perf::now() - startTime;
    return duration >= allotedTime;
  }

  void
  AddResult(ChessBoard pos, Score eval, Move pv[])
  {
    pvLength = 0;

    for (int i = 0; i < MAX_PV_ARRAY_SIZE; i++)
    {
      if (!IsLegalMoveForPosition(pv[i], pos) or pvLength >= MAX_PLY)
        break;
      pvLine[pvLength++] = pv[i];
      pos.MakeMove(pv[i]);
    }
    moveEvals.emplace_back(make_pair(pv[0], eval * (2 * side - 1)));
  }

  void
  SearchCompleted() noexcept
  {
    perf_time duration = perf::now() - startTime;
    timeForSearch = duration.count();
  }

  std::pair<Move, Score> LastIterationResult() const noexcept
  { return moveEvals.back(); }

  string
  GetSearchResult(ChessBoard board)
  {
    const auto&[move, eval] = moveEvals.back();
    const double eval_conv = static_cast<double>(eval) / 100.0;

    string result = "------ SEARCH-INFO ------";

    result +=
      + "\nDepth Searched = " + to_string(moveEvals.size() - 1)
      + "\nBest Move = " + PrintMove(move, board)
      + "\nEval = " + to_string(eval_conv)
      + "\nLine = " + ReadablePvLine(board)
      + "\nTime_Spend = " + to_string(timeForSearch) + " sec."
      + "\n-------------------------";
    
    return result;
  }

  // Prints the results of last searched depth
  string
  ShowLastDepthResult(ChessBoard board) const noexcept
  {
    const size_t dep = (moveEvals.size() - 1);
    const auto& [move, eval] = moveEvals.back();
    const double eval_conv = static_cast<double>(eval) / 100.0;
    const string gap = (dep < 10) ? (" ") : ("");

    string result = "Depth " + gap + to_string(dep) + " | ";

    result +=
        "Eval = " + to_string(eval_conv) + "\t| "
      + "Pv = " + ReadablePvLine(board);
    return result;
  }

  void
  SortMovesOnTime(Move bestMove)
  {
    for (size_t i = 0; i < moveCount; i++)
    {
      if (filter(bestMove) == filter(moveTimes[i].first))
      {
        std::swap(moveTimes[i], moveTimes[0]);
        break;
      }
    }

    std::sort(moveTimes + 1, moveTimes + moveCount, [](const auto &a, const auto &b) {
      return a.second > b.second;
    });
  }

  void Print(ChessBoard pos)
  {
    for (size_t i = 0; i < moveCount; i++)
    {
      const auto&[move, tm] = moveTimes[i];
      cout << (i + 1) << "\t| " << PrintMove(move, pos) << "\t| " << tm << endl;
    }
  }

  void
  InsertMoveToList(size_t moveNo, uint64_t timeOnMove)
  { moveTimes[moveNo].second = timeOnMove; }

  MoveList
  getMoves () const
  {
    MoveList myMoves(side, false);

    for (size_t i = 0; i < moveCount; i++)
      myMoves.Add(moveTimes[i].first);

    return myMoves;
  }
};

void OrderMoves(const ChessBoard& pos, MoveList& myMoves, bool pv_moves, bool check_moves);

Score
SeeScore(const ChessBoard& pos, Move move);

/**
 * @brief Prints all encoded-moves in list to human-readable strings
 * 
 * @param myMoves Movelist for board positions.
 * @param pos board position
 */
void
PrintMovelist(MoveList myMoves, ChessBoard pos);

extern SearchData info;

#endif
