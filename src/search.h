
#ifndef SEARCH_H
#define SEARCH_H

#include <iomanip>
#include <atomic>
#include "perf.h"
#include "varray.h"
#include "bitboard.h"
#include "movegen.h"
#include "search_utils.h"
#include "move_utils.h"

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
    const vector<string> values = utils::split(str, '|');
    fen = values[0].substr(0, values[0].length() - 1);
    if (testType == "accuracy") {
      depth = 0;
      const auto nodesStr = utils::split(values[1], ' ');
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

// Set by the UCI thread and checked by the search thread.
extern std::atomic<bool> searchStop;

// Orders moves for a search stage. History ordering can be disabled when the
// caller knows the stage will be skipped.
size_t
orderMoves(const ChessBoard& pos, MoveArray& movesArray, MType moveTypes, Ply ply,
           size_t start = 0, bool useHistory = true);

class SearchData
{
  perf_clock startTime;

  Color side;

  uint64_t nodes, qNodes;

  // Nodes searched so far over all iterations, main search and quiescence.
  // resetNodeCount() doesn't clear it. It feeds the UCI nodes and nps fields,
  // which GUIs expect to be cumulative.
  Nodes searchedNodes = 0;

  nanoseconds allotedTime;

  double timeForSearch = 0;  // seconds

  // shouldStop() runs several times per node, and reading the clock that often
  // is slow. So the clock is read once every CLOCK_POLL_INTERVAL calls and the
  // result is cached in timedOut. Once time is up it stays up, so the cached
  // value is never wrong for long. Both reset with each new SearchData.
  // Mutable because shouldStop() is const.
  mutable int pollCountdown = CLOCK_POLL_INTERVAL;
  mutable bool timedOut = false;

  public:

  // TT counters for the whole search:
  //   ttProbes:  nodes that probed the TT
  //   ttHits:    probes that found the position
  //   ttCutoffs: hits deep enough to return a score
  uint64_t ttProbes = 0, ttHits = 0, ttCutoffs = 0;

  // The same counters for quiescence. They are kept separate because a q-node
  // probes at depth 0, which every stored entry passes. So here cutoffs/hits
  // only measures whether the bound was usable, while in the main search it is
  // mostly the depth test.
  uint64_t qTtProbes = 0, qTtHits = 0, qTtCutoffs = 0;

  // Hash move counters:
  //   ttMoveProvided:  nodes where the TT gave a best move
  //   hashMoveInList:  of those, the move was legal and searched first
  //   hashMoveCutoffs: of those, the hash move alone caused a beta cutoff
  uint64_t ttMoveProvided = 0, hashMoveInList = 0, hashMoveCutoffs = 0;

  // PV nodes that had a usable TT cutoff but searched anyway, since PV nodes
  // don't take TT cutoffs. Compare with ttCutoffs to see what that costs.
  uint64_t pvTtCutoffsDeclined = 0;

  // PVS counters:
  //   pvsScouts:     later moves searched with a null window
  //   pvsResearches: scouts that beat alpha and were searched again
  // A high researches/scouts ratio means move ordering puts bad moves first.
  uint64_t pvsScouts = 0, pvsResearches = 0;

  private:

  Varray<Move, MAX_PLY> pvLine;

  // Length of pvLine before extendPvFromTt() added moves from the TT.
  // isPartOfPv() only looks at this part, so moves added for display don't
  // change move ordering.
  size_t pvSearchedLen = 0;

  Varray<pair<Move, Score>, MAX_DEPTH + 1> moveEvals;

  // Stores <move, <nodes, qNodes>> searched for each move in each iteration.
  Varray<pair<Move, pair<Nodes, Nodes>> , MAX_MOVES> moveNodes;

  string
  ReadablePvLine(ChessBoard board) const noexcept
  {
    string res;
    bool qmovesFound = false;

    for (const Move move : pvLine)
    {
      if ((move & quiescenceMove()) and !qmovesFound)
      {
        res += "(";
        qmovesFound = true;
      }

      res += printMove(move, board) + string(" ");
      board.makeMove(move);
    }

    if (qmovesFound)
      res[res.size() - 1] = ')';

    return res;
  }

  // Fill in the end of a PV that pvArray cut short.
  //
  // pvArray only gets a move from a node that searched its moves and raised
  // alpha. A node that returns early (a TT cutoff, a draw, RFP, razoring, NMP)
  // passes up a score with no move, so the printed PV stops there even though
  // the score comes from the full depth.
  //
  // The TT still has those moves, so follow the table's best moves on from the
  // end of the PV. probePvMove() only returns a move from an exact entry
  // searched to at least the depth left at that point. Without that check, one
  // wrong move would send the walk through positions that were never on the
  // PV. A short PV is better than a wrong one.
  //
  // The added moves are only for display (see pvSearchedLen).
  void
  extendPvFromTt(ChessBoard pos, Depth rootDepth)
  {
    // With the TT off nothing is allocated, and a probe would read through a
    // null pointer.
    if constexpr (!USE_TT)
      return;

    // A PV that ends in quiescence is complete. Adding main-search moves would
    // put them inside the "(...)" quiescence part of the printed PV.
    if (pvLine.size() > 0 and (pvLine.back() & quiescenceMove()))
      return;

    while (pvLine.size() < pvLine.capacity())
    {
      // Stop at a 50-move or repetition draw. The TT can't tell: the hash key
      // has neither the halfmove clock nor the game history, so an entry from
      // another path passes probePvMove()'s checks. This also stops the walk
      // from bouncing between two positions that store each other's move.
      // `pos` is a copy with the full undo stack, so threeMoveRepetition() sees
      // the PV so far and the game before the root too.
      if (pos.fiftyMoveDraw() or pos.threeMoveRepetition())
        break;

      // Depth left at this ply. pvLine.size() is the current ply, and a node on
      // this iteration's PV was searched to at least rootDepth - ply
      // (extensions only add depth, and the PV is never reduced). Asking for
      // that much rejects entries left by shallower iterations.
      //
      // Don't clamp this to 1 to walk past rootDepth. A floor of 1 lets depth-1
      // entries back in, and those produce fake moves.
      const Depth remaining = rootDepth - Depth(pvLine.size());
      if (remaining < 1)
        break;

      const Move move = tt.probePvMove(pos.hashValue, remaining);

      // Stop if there's no entry, the entry isn't exact or deep enough, or the
      // move isn't legal here (a hash key collision).
      if (move == NULL_MOVE or !isLegalMoveForPosition(move, pos))
        break;

      pvLine.push(move);
      pos.makeMove(move);
    }
  }

  public:

  SearchData()
  : startTime(perf::now()) {}

  SearchData(ChessBoard& pos, double _allotedTime)
  : startTime(perf::now()), side(pos.color), nodes(0), qNodes(0),
    allotedTime(std::chrono::duration_cast<nanoseconds>(std::chrono::duration<double>(_allotedTime)))
  {
    // `true` also generates the check data that MType::CHECK ordering needs.
    const MoveList myMoves = generateMoves(pos, true);
    MoveArray movesArray;
    myMoves.getMoves(pos, movesArray);

    // The first iteration has no results to order by, so order the root moves
    // the same way as the rest of the tree. Order one stage at a time, as
    // playAllMoves does. A single combined mask would skip the bad-capture
    // demotion (only done when mTypes == MType::CAPTURES) and would SEE-sort
    // captures, promotions and checks as one group.
    //
    // PV and KILLER are left out. is_type<MType::PV> reads the global `info`,
    // which still holds the previous search, and there's no PV yet anyway. The
    // killer table was just cleared. History was cleared too, so useHistory is
    // false.
    size_t start = 0;
    start = orderMoves(pos, movesArray, MType::CAPTURES,  0, start, false);
    start = orderMoves(pos, movesArray, MType::PROMOTION, 0, start, false);
            orderMoves(pos, movesArray, MType::CHECK,     0, start, false);

    Move zeroMove = movesArray[0];
    moveEvals.push(make_pair(zeroMove, VALUE_ZERO));

    for (const Move move : movesArray)
      moveNodes.push(make_pair(move, make_pair(0, 0)));
  }

  // The PV built by addResult, with every move checked for legality. The UCI
  // info line prints this instead of the raw pvArray, whose tail can be stale.
  const Varray<Move, MAX_PLY>&
  getPvLine() const noexcept
  { return pvLine; }

  bool
  isPartOfPv(const Move m) const noexcept
  {
    const Move filteredMove = filter(m);

    // Only moves from the searched part of the PV affect move ordering.
    for (size_t i = 0; i < pvSearchedLen; i++) {
      if (filter(pvLine[i]) == filteredMove)
        return true;
    }
    return false;
  }

  bool
  timeOver() const noexcept
  {
    nanoseconds duration = perf::now() - startTime;
    return duration >= allotedTime;
  }

  // True when the time is up or the UCI thread asked to stop. The search
  // checks this instead of timeOver() so that `stop` and `go infinite` work.
  // searchStop is cheap to read, so it is checked on every call and `stop`
  // takes effect at once. Only the clock read is throttled.
  bool
  shouldStop() const noexcept
  {
    if (searchStop.load(std::memory_order_relaxed))
      return true;

    if (--pollCountdown > 0)
      return timedOut;

    pollCountdown = CLOCK_POLL_INTERVAL;
    timedOut = timeOver();
    return timedOut;
  }

  double
  timeSpent() const noexcept
  {
    nanoseconds duration = perf::now() - startTime;
    return double(duration.count()) / 1e9;
  }

  void
  addResult(ChessBoard pos, Score eval, Move pv[], Depth depth)
  {
    pvLine.clear();

    // Bounded by pvLine's capacity, not MAX_PV_ARRAY_SIZE. The root's row in
    // pvArray is only MAX_PLY entries long, so reading further would run into
    // the ply-1 row.
    for (size_t i = 0; i < pvLine.capacity(); i++)
    {
      if (!isLegalMoveForPosition(pv[i], pos))
        break;
      pvLine.push(pv[i]);
      pos.makeMove(pv[i]);
    }

    // Everything so far was searched. Record its length before adding moves
    // from the TT, so move ordering only uses the searched part.
    pvSearchedLen = pvLine.size();

    // `pos` is now at the end of the PV. Walk the TT from here to recover the
    // moves that early-returning nodes didn't write to pvArray. The walk needs
    // this iteration's depth to check each entry.
    extendPvFromTt(pos, depth);

    moveEvals.push(make_pair(pv[0], eval * (2 * side - 1)));
  }

  void
  searchCompleted() noexcept
  {
    perf_time duration = perf::now() - startTime;
    timeForSearch = duration.count();
  }

  void
  addNode() noexcept
  { nodes++; searchedNodes++; }

  void
  addQNode() noexcept
  { qNodes++; searchedNodes++; }

  void
  resetNodeCount() noexcept
  { nodes = 0; qNodes = 0; }

  pair<Move, Score> lastIterationResult() const noexcept
  { return moveEvals.back(); }

  // The move to play. search() keeps the best move at the front of the root
  // list with promoteBestMove, including one from an unfinished iteration.
  Move
  bestMoveFound() const noexcept
  { return moveNodes[0].first; }

  Nodes
  totalSearchedNodes() const noexcept
  { return searchedNodes; }

  // Nodes per second over the whole search so far. Handles zero elapsed time,
  // which can happen on very fast first iterations.
  Nodes
  nps() const noexcept
  {
    const double elapsed = timeSpent();
    return elapsed > 0.0 ? Nodes(double(searchedNodes) / elapsed) : searchedNodes;
  }

  Nodes
  totalNodes() const noexcept
  {
    return std::accumulate(
      moveNodes.begin(), moveNodes.end(), Nodes(0),
      [](Nodes sum, const pair<Move, pair<Nodes, Nodes>>& entry) {
        return sum + entry.second.first;
      }
    );
  }

  Nodes
  totalQNodes() const noexcept
  {
    return std::accumulate(
      moveNodes.begin(), moveNodes.end(), Nodes(0),
      [](uint64_t sum, const pair<Move, pair<Nodes, Nodes>>& entry) {
        return sum + entry.second.second;
      }
    );
  }

  void
  showLastDepthResult(ChessBoard pos, std::ostream& writer) const noexcept
  {
    using std::setw, std::right, std::fixed, std::setprecision;

    const size_t dep = moveEvals.size() - 1;
    const Score eval = moveEvals.back().second;
    double evalConv  = double(eval) / 100.0;

    writer << " | " << setw(6) << right << fixed << setprecision(2) << timeSpent()
           << " | " << setw(5) << right << fixed << dep
           << " | " << setw(7) << right << fixed << setprecision(2) << evalConv
           << " | " << setw(8) << right << fixed << totalNodes()
           << " | " << setw(8) << right << fixed << totalQNodes()
           << " | " << ReadablePvLine(pos) << endl;
  }

  void
  showHeader(std::ostream& writer) const noexcept
  {
    using std::setw;
    writer << " | " << setw(6) << "Time"
           << " | " << setw(5) << "Depth"
           << " | " << setw(7) << "Score"
           << " | " << setw(8) << "Nodes"
           << " | " << setw(8) << "QNodes"
           << " | " << "PV" << "\n";
  }

  // Move bestMove to the front of the root list for the next iteration and
  // keep the other moves in order. Don't sort the rest by node count. A small
  // subtree can mean a bad move that was refuted quickly, or a good move whose
  // subtree cut off early. Root LMR reduces by list position (up to R=3 at
  // depth >= 6), so a good move pushed to the back would lose depth. Node
  // counts are still recorded for printing.
  void
  promoteBestMove(Move bestMove)
  {
    // On an aspiration fail-low no move beat alpha, so rootAlphaBeta never
    // wrote pvArray[0]. Use the best move of the last completed iteration.
    // moveEvals is seeded in the constructor, so back() is always a legal move.
    if (bestMove == NULL_MOVE)
    {
      if (moveEvals.size() == 0)
        return;
      bestMove = moveEvals.back().first;
    }

    for (size_t i = 0; i < moveNodes.size(); i++)
    {
      if (filter(bestMove) == filter(moveNodes[i].first))
      {
        // Rotate instead of swap so the other moves keep their order. This
        // moves element i to the front and shifts 0..i-1 right by one.
        std::rotate(moveNodes.begin(), moveNodes.begin() + i, moveNodes.begin() + i + 1);
        break;
      }
    }
  }

  void
  print(ChessBoard pos)
  {
    using std::setw, std::right, std::fixed;
    cout << " | " << setw( 6) << "moveNo"
         << " | " << setw( 5) << "Move"
         << " | " << setw( 8) << "Nodes"
         << " | " << setw(10) << "QNodes |\n";
    int moveNo = 1;
    for (const auto& [move, nc] : moveNodes)
    {
      cout << " | " << setw(6) << fixed << moveNo++
           << " | " << setw(5) << fixed << printMove(move, pos)
           << " | " << setw(8) << fixed << nc.first
           << " | " << setw(7) << fixed << nc.second << " |\n";
    }
  }

  void
  insertMoveToList(size_t moveNo)
  {
    moveNodes[moveNo].second = make_pair(nodes, qNodes);
    resetNodeCount();
  }

  MoveArray
  getMoves () const
  {
    MoveArray movesArray;

    for (const auto& moveTime : moveNodes)
      movesArray.push(moveTime.first);

    return movesArray;
  }
};

// Sorts a capture list for quiescence by SEE, best first, in place. The first
// `floor` moves are kept even if their SEE is negative. Returns how many moves
// from the front should be searched.
size_t
orderCaptures(const ChessBoard& pos, MoveArray& movesArray, size_t floor);

Score
seeScore(const ChessBoard& pos, Move move);

void
printMovelist(MoveArray myMoves, ChessBoard pos);

extern SearchData info;

#endif
