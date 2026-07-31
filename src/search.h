
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

// Control-plane abort signal raised by the UCI `stop`/`quit` handlers (main
// thread) and polled by the search worker via SearchData::shouldStop(). Lives
// outside SearchData because std::atomic is non-copyable and `info` is
// rebuilt by copy-assignment (`info = SearchData(...)`) on every search.
extern std::atomic<bool> searchStop;

class SearchData
{
  // Set the starting point for clock
  perf_clock startTime;

  // Store which side to play for search_position
  Color side;

  uint64_t nodes, qNodes;

  // Cumulative node count over the entire search (all depths, main + q).
  // Unlike `nodes`/`qNodes` this is NEVER cleared by resetNodeCount(), so it
  // feeds the UCI `nodes`/`nps` fields, which GUIs expect to be cumulative.
  Nodes searchedNodes = 0;

  // Time provided to find move for current position
  nanoseconds allotedTime;

  // Time spend on searching for move in position (in secs.)
  double timeForSearch = 0;

  public:

  // Transposition-table instrumentation, accumulated over the whole search:
  //   ttProbes   — nodes that probed the TT
  //   ttHits     — of those, the position was found (hash match)
  //   ttCutoffs  — of those hits, the entry was deep enough to return a bound
  uint64_t ttProbes = 0, ttHits = 0, ttCutoffs = 0;

  // Hash-move (TT best move) instrumentation, accumulated over the whole search:
  //   ttMoveProvided   — nodes where the TT handed back a usable best move
  //   hashMoveInList   — of those, the move was legal here and tried first
  //   hashMoveCutoffs  — of those, the hash move alone produced a beta cutoff
  uint64_t ttMoveProvided = 0, hashMoveInList = 0, hashMoveCutoffs = 0;

  private:

  Varray<Move, MAX_PLY> pvLine;

  // How much of pvLine the search actually walked, i.e. the length before
  // extendPvFromTt() appended its tail. isPartOfPv() — which drives the
  // MType::PV ordering bucket via is_type<MType::PV> — must stay bounded by
  // this: the TT tail is reconstructed for display, and letting it widen the
  // set of moves promoted in ordering would make a cosmetic fix perturb the
  // search at every node.
  size_t pvSearchedLen = 0;

  // Stores the <best_move, eval> for each depth during search.
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

  // Rebuild the tail of a PV that pvArray truncated.
  //
  // The triangular pvArray only records moves at nodes that actually searched
  // moves and raised alpha. A node that returns early — a TT cutoff above all,
  // but also the draw / RFP / razoring / NMP exits — hands its parent a score
  // with no move attached, so the printed line stops dead there (a depth-11
  // search showing three moves). The score is still a real depth-11 score; the
  // moves are just missing from the display.
  //
  // They aren't lost, though: the node that cut off has a TT entry, and so does
  // every node beyond it, written by whichever iteration searched them for real.
  // So walk the table's best moves onward from the position the raw PV ended at.
  //
  // The walk is *verified*: probePvMove() only answers from an exact-bound entry
  // searched to at least the depth still remaining at that point in the line, so
  // the tail stops at the first link the table cannot vouch for. An unverified
  // walk chains — one unproven move and every later probe describes a position
  // that was never on the PV, which is how this used to print a whole fabricated
  // queen trade off the end of a nine-ply line. A short honest tail beats a long
  // invented one; see docs/pv-reconstruction.md.
  //
  // The appended moves are for display only — see pvSearchedLen for why they
  // are fenced off from isPartOfPv().
  void
  extendPvFromTt(ChessBoard pos, Depth rootDepth)
  {
    // With the TT disabled there is no table to walk — and TT_SIZE is 0, so
    // probeMove()'s `hash % TT_SIZE` would divide by zero.
    if constexpr (!USE_TT)
      return;

    // A line that ended inside quiescence ended *naturally*; appending
    // main-search moves would splice them into the "(...)" tail of the
    // readable PV, misrepresenting them as quiescence moves.
    if (pvLine.size() > 0 and (pvLine.back() & quiescenceMove()))
      return;

    // Repetition guard. Two positions can each store the other's move as best
    // (a king/rook shuffle), and the walk would happily bounce between them
    // until it filled the line to capacity with a fake PV.
    Varray<uint64_t, MAX_PLY + 1> seen;
    seen.add(pos.hashValue);

    const auto alreadySeen = [&seen] (uint64_t key) {
      for (const uint64_t k : seen)
        if (k == key) return true;
      return false;
    };

    while (pvLine.size() < pvLine.capacity())
    {
      // Depth still owed at this point in the line. pvLine.size() is exactly the
      // ply we are standing on (the prefix moves have all been made on `pos`), so
      // a node genuinely on this iteration's PV was searched at `rootDepth - ply`
      // -- extensions only ever push that higher, and reductions never apply on
      // the PV, so demanding at least this much admits the real entries and
      // rejects leftovers from shallower iterations.
      const Depth remaining = rootDepth - Depth(pvLine.size());
      if (remaining < 1)
        break;

      const Move move = tt.probePvMove(pos.hashValue, remaining);

      // No entry (the position was never stored, e.g. a terminal node), no entry
      // the table will vouch for at this depth/bound, or the entry belongs to a
      // 64-bit key collision and its move is nonsense here.
      if (move == NULL_MOVE or !isLegalMoveForPosition(move, pos))
        break;

      pvLine.add(move);
      pos.makeMove(move);

      if (alreadySeen(pos.hashValue))
        break;
      seen.add(pos.hashValue);
    }
  }

  public:

  SearchData()
  : startTime(perf::now()) {}

  SearchData(ChessBoard& pos, double _allotedTime)
  : startTime(perf::now()), side(pos.color), nodes(0), qNodes(0),
    allotedTime(std::chrono::duration_cast<nanoseconds>(std::chrono::duration<double>(_allotedTime)))
  {
    const MoveList myMoves = generateMoves(pos);
    MoveArray movesArray;
    myMoves.getMoves(pos, movesArray);
    Move zeroMove = movesArray[0];
    moveEvals.add(make_pair(zeroMove, VALUE_ZERO));

    for (const Move move : movesArray)
      moveNodes.add(make_pair(move, make_pair(0, 0)));
  }

  // Read access to the validated principal variation (built by addResult;
  // every move legality-checked). Used by the UCI info printer so it emits a
  // legal PV instead of walking the raw pvArray (whose tail can be stale).
  const Varray<Move, MAX_PLY>&
  getPvLine() const noexcept
  { return pvLine; }

  bool
  isPartOfPv(const Move m) const noexcept
  {
    const Move filteredMove = filter(m);

    // Searched prefix only — the TT-reconstructed tail must not reach ordering.
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

  // Abort predicate polled at every search checkpoint: true when the time
  // budget is spent OR the UCI layer asked to stop. Used in place of
  // timeOver() at the abort gates so `stop` (and `go infinite`) work.
  bool
  shouldStop() const noexcept
  { return timeOver() || searchStop.load(std::memory_order_relaxed); }

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

    // Bounded by pvLine's capacity, not MAX_PV_ARRAY_SIZE: the root's row in
    // the triangular pvArray is only the first MAX_PLY entries, so a full-length
    // legal line would otherwise run off it into the ply-1 row (and the extra
    // moves would be silently dropped by Varray::add anyway).
    for (size_t i = 0; i < pvLine.capacity(); i++)
    {
      if (!isLegalMoveForPosition(pv[i], pos))
        break;
      pvLine.add(pv[i]);
      pos.makeMove(pv[i]);
    }

    // Everything up to here was genuinely searched — freeze that bound before
    // the TT tail goes on, so move ordering only ever sees the searched prefix.
    pvSearchedLen = pvLine.size();

    // `pos` now sits at the end of the raw line — walk the TT onward from here
    // to recover the moves an early-returning node never wrote to pvArray.
    // `depth` is the iteration that produced this line; the walk needs it to
    // know how much search each recovered move still has to be backed by.
    extendPvFromTt(pos, depth);

    moveEvals.add(make_pair(pv[0], eval * (2 * side - 1)));
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

  // Cumulative nodes (main + quiescence) searched so far, across all depths.
  Nodes
  totalSearchedNodes() const noexcept
  { return searchedNodes; }

  // Nodes per second over the whole search so far. Guards against a zero
  // elapsed time on very fast first iterations.
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

  // Prints the results of last searched depth
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

  // Reorder root moves for the next iteration: keep the PV move first, then
  // order the rest by descending subtree size (2*nodes + qNodes) so the
  // hardest-to-resolve moves are searched earliest.
  void
  sortMovesOnNodes(Move bestMove)
  {
    for (size_t i = 0; i < moveNodes.size(); i++)
    {
      if (filter(bestMove) == filter(moveNodes[i].first))
      {
        std::swap(moveNodes[i], moveNodes[0]);
        break;
      }
    }

    std::sort(moveNodes.begin() + 1, moveNodes.end(), [](const auto &a, const auto &b) {
      Nodes n1 = 2 * a.second.first + a.second.second;
      Nodes n2 = 2 * b.second.first + b.second.second;
      return n1 > n2;
    });
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
      movesArray.add(moveTime.first);

    return movesArray;
  }
};

size_t
orderMoves(const ChessBoard& pos, MoveArray& movesArray, MType moveTypes, Ply ply, size_t start = 0);

/**
 * @brief SEE-orders a pure capture list for quiescence search.
 *
 * @param pos board position
 * @param movesArray capture-only move list, reordered in place (SEE descending)
 * @param floor moves to keep even when their SEE is negative
 * @return count of leading moves worth searching
 */
size_t
orderCaptures(const ChessBoard& pos, MoveArray& movesArray, size_t floor);

Score
seeScore(const ChessBoard& pos, Move move);

/**
 * @brief Prints all encoded-moves in list to human-readable strings
 *
 * @param myMoves Movelist for board positions.
 * @param pos board position
 */
void
printMovelist(MoveArray myMoves, ChessBoard pos);

extern SearchData info;

#endif
