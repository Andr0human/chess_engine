
#include <unordered_map>
#include <functional>
#include <set>
#include "task.h"
#include "search.h"
#include "single_thread.h"
#include "test_positions.h"

void
init(const vector<string>& args)
{
  perf_clock start = perf::now();
  plt::init();

  if constexpr (USE_TT) {
    tt.resize(0);
  }
  perf_time dur = perf::now() - start;
  const auto it = dur.count();

  bool sec = (it >= 1);

  if (utils::hasArg(args, "debug"))
  {
    cout << "Table Gen. took " << (sec ? it : it * 1000)
         << (sec ? " s.\n" : " ms.") << endl;

    if constexpr (USE_TT) {
      cout << "Transposition Table Size = " << tt.size()
          << "\n\n" << std::flush;
    }
  }
}

static vector<TestPosition>
getTestPositions(const std::vector<std::string>& suite, const std::string& testType)
{
  vector<TestPosition> testPositions;

  for (const auto& testData : suite)
    testPositions.emplace_back(TestPosition(testData, testType));

  return testPositions;
}

static void
accuracyTest()
{
  // Argument : elsa accuracy

  const auto runTests = [] (const vector<TestPosition>& positions)
  {
    int posNo = 1;
    uint64_t totalNs = 0;

    for (const auto& pos : positions)
    {
      const auto depth = pos.maxDepth();

      const auto start = perf::now();
      const bool passed = pos.test(bulkCount, depth);
      const auto ns = std::chrono::duration_cast<perf_ns_time>(
        perf::now() - start
      ).count();

      totalNs += static_cast<uint64_t>(ns);

      if (!passed)
        return std::make_pair(false, totalNs);

      cout << "Position " << (posNo++) << " passed!"
           << "  (" << std::fixed << std::setprecision(1)
           << static_cast<double>(ns) / 1e6 << " ms)" << std::endl;
    }

    const double avgMs = static_cast<double>(totalNs)
                       / (static_cast<double>(positions.size()) * 1e6);
    cout << "\nOverall accuracy timing:\n"
         << "  Total   : " << std::fixed << std::setprecision(1)
         << static_cast<double>(totalNs) / 1e6 << " ms\n"
         << "  Avg/pos : " << avgMs << " ms\n";

    return std::make_pair(true, totalNs);
  };

  const auto failedTests = [] (const vector<TestPosition>& positions)
  {
    TestPosition bestCase = positions.front();
    Depth minDepth = 100;

    for (const auto& pos : positions)
    {
      Depth depth = pos.maxDepth();
      for (Depth dep = 1; dep <= depth; dep++)
      {
        if (pos.test(bulkCount, dep))
          continue;

        if (dep < minDepth)
        {
          bestCase = pos, minDepth = dep;
          break;
        }
      }
    }
    return bestCase;
  };

  const auto positions = getTestPositions(test_data::accuracy::suite1, "accuracy");
  const auto [testSuccess, _] = runTests(positions);

  if (testSuccess) return;

  cout << "Accuracy Test Failed!!\n" 
          "Looking for best_case:\n\n" << std::flush;

  const auto bestCase = failedTests(positions);

  cout << "Best Failed Case : \n";
  cout << "Fen = " << bestCase.getFen() << endl;
}

static void
hashLegalTest()
{
  // Argument : elsa hashlegal
  //
  // Equivalence / fuzz test for isHashMoveLegal(). For each position in the
  // suite below we build the reference set of legal moves (keyed on the 20 bits
  // isHashMoveLegal actually consults — ip/fp/pIp/pFp/promo) and then check:
  //   * every legal move is accepted              (no false negatives)
  //   * no other move is accepted                 (no false positives)
  //   * anything accepted survives a makeMove / integrityCheck / unmakeMove
  //
  // Run this before wiring isHashMoveLegal into the search.

  // ---- gather FENs: the perft suites + a hand-picked set of delicate cases ----
  std::vector<std::string> fens;

  const auto addPerftSuite = [&] (const std::vector<std::string>& suite)
  {
    for (const auto& entry : suite)
      fens.push_back(utils::strip(utils::split(entry, '|').front()));
  };
  addPerftSuite(test_data::accuracy::suite1);
  addPerftSuite(test_data::speed::suite1);

  const std::vector<std::string> tricky = {
    "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",                      // castling: all four sides free
    "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "4k3/8/8/8/8/8/4r3/R3K2R w KQ - 0 1",                        // white in check on the e-file -> no castling
    "r3k2r/8/8/8/8/5b2/8/R3K2R w KQkq - 0 1",                    // f3 bishop attacks g2/h1... and the queenside path
    "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3",   // e.p. available (exd6)
    "4k3/8/8/2Pp4/8/8/8/4K3 w - d6 0 1",                         // plain e.p.
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                 // famous e.p. discovered-check setup
    "8/8/3p4/KPp4r/1R3p1k/8/4P1P1/8 w - c6 0 1",                 // ...with the e.p. square actually set
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", // d7 pawn: push-promo + capture-promos
    "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",                   // a mess of promotions and capture-promotions
    "rnbqkbnr/1ppppppp/p7/8/Q7/8/PPPPPPPP/RNB1KBNR b KQkq - 1 2",// black in check from Qa4 -> block / capture / Kd... no
    "8/8/8/3k4/3Pp3/8/8/3KR3 b - d3 0 1",                        // pinned-ish pawn + e.p. interplay
    "4k3/8/8/8/8/8/3p4/4K3 b - - 0 1",                           // black pawn one step from promotion, blocked path
    "rnb1kbnr/pp1ppppp/8/q1p5/8/2P5/PP1PPPPP/RNBQKBNR w KQkq - 1 3",  // Qa5 pins... actually checks? no — diag to e1? no
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", // dense, pins galore, a7 push-promos
    "8/8/8/8/k2Pp2Q/8/8/4K3 b - d3 0 1",                         // black e4 pawn pinned by Qh4 along the 4th rank
    "8/8/8/8/k1pP3R/8/8/4K3 b - d3 0 1",                         // e.p. capture by c4 would expose Rh4 -> illegal
    START_FEN,
  };
  for (const auto& fen : tricky)
    fens.push_back(fen);

  // ---- simple deterministic PRNG for fuzzed candidates ----
  uint64_t rngState = 0x9E3779B97F4A7C15ULL;
  const auto nextRand = [&] () -> uint64_t
  {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 7;
    rngState ^= rngState << 17;
    return rngState;
  };

  size_t  posCount = 0;
  size_t  refMoveTotal = 0;
  size_t  candidatesChecked = 0;
  size_t  falseNegatives = 0;
  size_t  falsePositives = 0;
  size_t  corruptions = 0;

  // Canonical identity of a move under makeMove semantics: ip|fp|pIp|pFp, plus
  // the promotion piece *only* when the destination is the promotion rank
  // (otherwise makeMove — and isHashMoveLegal — never look at those two bits).
  const auto canonKey = [] (Move m) -> Move
  {
    Move k = m & 0x3FFFF;
    if (((m >> 12) & 7) == PAWN && ((1ULL << to_sq(m)) & Rank18))
      k |= m & 0xC0000;
    return k;
  };

  const auto reportFail = [] (const char* what, const std::string& fen, Move m)
  {
    cout << "  [FAIL] " << what
         << "  fen = " << fen
         << "  move = 0x" << std::hex << m << std::dec
         << "  (" << char('a' + (m & 7)) << char('1' + ((m >> 3) & 7))
         << char('a' + ((m >> 6) & 7)) << char('1' + ((m >> 9) & 7)) << ")\n";
  };

  for (const auto& fen : fens)
  {
    ChessBoard pos(fen);
    ++posCount;

    MoveList meta;
    stagedGenerateMoves<GEN_METADATA>(pos, meta);

    MoveList full = generateMoves(pos);
    MoveArray legalMoves;
    full.getMoves(pos, legalMoves);

    std::set<Move> ref;
    for (const Move m : legalMoves)
      ref.insert(canonKey(m));
    refMoveTotal += ref.size();

    // ---- (a) no false negatives: every generated legal move must be accepted ----
    for (const Move m : legalMoves)
    {
      ++candidatesChecked;
      if (!isHashMoveLegal(m, pos, meta))
      {
        ++falseNegatives;
        reportFail("false negative (legal move rejected)", fen, m);
      }
    }

    const auto vet = [&] (Move c)
    {
      ++candidatesChecked;
      if (!isHashMoveLegal(c, pos, meta))
        return;
      if (ref.find(canonKey(c)) == ref.end())
      {
        ++falsePositives;
        reportFail("false positive (illegal move accepted)", fen, c);
        return;
      }
      // accepted *and* genuinely legal — confirm makeMove keeps the board sane
      pos.makeMove(c);
      const bool ok = pos.integrityCheck() && (pos.hashValue == pos.generateHashkey());
      pos.unmakeMove();
      if (!ok)
      {
        ++corruptions;
        reportFail("makeMove corrupted the board", fen, c);
      }
    };

    // ---- (b) structured sweep: every (ip, fp) starting from one of our pieces,
    //          correct pIp/pFp, and (for pawns) every promotion piece ----
    Move colorBit = Move(pos.color) << 22;
    for (int ipi = 0; ipi < SQUARE_NB; ++ipi)
    {
      Piece ipPiece = pos.pieceOnSquare(Square(ipi));
      if (ipPiece == NO_PIECE || color_of(ipPiece) != pos.color)
        continue;
      PieceType ipt = type_of(ipPiece);

      for (int fpi = 0; fpi < SQUARE_NB; ++fpi)
      {
        if (fpi == ipi) continue;
        PieceType fpt = type_of(pos.pieceOnSquare(Square(fpi)));
        Move base = colorBit | (Move(ipt) << 12) | (Move(fpt) << 15)
                  | (Move(fpi) << 6) | Move(ipi);

        if (ipt == PAWN)
          for (Move promo = 0; promo < 4; ++promo)
            vet(base | (promo << 18));
        else
          vet(base);

        // a couple of deliberately-corrupt encodings (wrong pIp / pFp)
        vet((base & ~(Move(7) << 12)) | (Move((ipt % 6) + 1) << 12));
        vet((base & ~(Move(7) << 15)) | (Move((fpt + 1) & 7) << 15));
      }
    }

    // ---- (c) near-misses of real legal moves: tweak one field by a little ----
    for (const Move m : legalMoves)
    {
      vet(m ^ Move(1));                       // ip +/- 1
      vet(m ^ (Move(1) << 6));                // fp +/- 1
      vet(m ^ (Move(1) << 12));               // flip a pIp bit
      vet(m ^ (Move(1) << 15));               // flip a pFp bit
      vet(m ^ (Move(3) << 18));               // flip promo bits
      vet(m ^ (Move(7) << 6));                // fp jumped by 7
    }

    // ---- (d) pure random 24-bit patterns ----
    for (int i = 0; i < 4000; ++i)
      vet(Move(nextRand() & 0xFFFFFF));
  }

  cout << "\nisHashMoveLegal fuzz test\n"
       << "  positions          : " << posCount << '\n'
       << "  legal moves (uniq) : " << refMoveTotal << '\n'
       << "  candidates checked : " << candidatesChecked << '\n'
       << "  false negatives    : " << falseNegatives << '\n'
       << "  false positives    : " << falsePositives << '\n'
       << "  board corruptions  : " << corruptions << '\n';

  if (falseNegatives == 0 && falsePositives == 0 && corruptions == 0)
    cout << "  RESULT             : PASS\n" << std::flush;
  else
    cout << "  RESULT             : FAIL\n" << std::flush;

  // ---- micro-benchmark: pinnedRayDest (8-dir scan) vs pinnedRayDest_fast ----
  // Same FEN list, every non-king own-side piece, kIters per position. The two
  // functions are timed back-to-back inside benchmarkPinnedRayDest; we sum
  // across positions to drown out single-call noise. Run a warm-up pass first.
  constexpr int kIters = 20000;

  double slowTotal = 0.0, fastTotal = 0.0;
  size_t pieceCallTotal = 0;

  for (const auto& fen : fens)
  {
    ChessBoard pos(fen);
    benchmarkPinnedRayDest(pos, 200);                 // warm-up, discarded
    const auto [slow, fast] = benchmarkPinnedRayDest(pos, kIters);
    slowTotal += slow;
    fastTotal += fast;

    const Bitboard sideMask = (pos.color == WHITE)
      ? (pos.piece<WHITE, PAWN>()  | pos.piece<WHITE, BISHOP>()
       | pos.piece<WHITE, KNIGHT>()| pos.piece<WHITE, ROOK>()
       | pos.piece<WHITE, QUEEN>())
      : (pos.piece<BLACK, PAWN>()  | pos.piece<BLACK, BISHOP>()
       | pos.piece<BLACK, KNIGHT>()| pos.piece<BLACK, ROOK>()
       | pos.piece<BLACK, QUEEN>());
    pieceCallTotal += size_t(__builtin_popcountll(sideMask)) * size_t(kIters);
  }

  const double slowNs = slowTotal * 1e9 / double(pieceCallTotal);
  const double fastNs = fastTotal * 1e9 / double(pieceCallTotal);
  const double speedup = (fastTotal > 0.0) ? (slowTotal / fastTotal) : 0.0;

  cout << "\npinnedRayDest micro-benchmark\n"
       << "  iters / position   : " << kIters << '\n'
       << "  total piece-calls  : " << pieceCallTotal << '\n'
       << "  pinnedRayDest      : " << slowTotal << " s  ("
       << slowNs << " ns/call)\n"
       << "  pinnedRayDest_fast : " << fastTotal << " s  ("
       << fastNs << " ns/call)\n"
       << "  speedup            : " << speedup << "x\n" << std::flush;
}

static void
helper()
{
  // Argument : elsa help

  puts("/****************   Command List   ****************/\n");

  puts("** For Elsa's movegenerator self-accuracy test, type:\n");
  puts("** elsa accuracy\n");
  
  puts("** For Elsa's movegenerator self-speed test, type:\n");
  puts("** elsa speed\n");

  puts("** For the isHashMoveLegal fuzz/equivalence test, type:\n");
  puts("** elsa hashlegal\n");

  puts("** For Bulk-Counting, type:\n");
  puts("** elsa count [fen <fen>] [depth <depth>]\n");

  puts("** For Evaluating a position, type:\n");
  puts("** elsa go [fen <fen>] [time <search_time>] [depth <depth>] [debug]\n");

  puts("** For debugging movegenerator, type:\n");
  puts("** elsa debug [fen <fen>] [depth <depth>] [output <filename>]\n");
  
  puts("** For static evaluation of a position, type:\n");
  puts("** elsa static [fen <fen>]\n");

  puts("** Note: Commands and flags can be in any order\n");
  puts("         (e.g. 'elsa debug depth 3 fen <fen>' or 'elsa fen <fen> go')\n");

  puts("/**************************************************/\n\n");
}

static void
speedTest()
{
  // Argument : elsa speed

  const auto positions = getTestPositions(test_data::speed::suite1, "speed");
  cout << "Positions Found : " << positions.size() << '\n';
  
  int64_t totalTime = 0;
  Nodes totalNodes = 0;
  const int loop = 3;
  int positionNo = 1;

  for (auto pos : positions)
  {
    Nodes currentNodes = pos.nodeCount(1) * loop;
    uint64_t currentTime = pos.time(bulkCount, loop);

    totalNodes += currentNodes;
    totalTime  += currentTime;

    const uint64_t currentSpeed = currentNodes / currentTime;
    cout << "position-" << positionNo++ << "\t: " << currentSpeed << " M nodes/sec." << endl;
  }

  const uint64_t speed = totalNodes / totalTime;
  cout << "Single Thread Speed : " << speed << " M nodes/sec." << endl;
}

static void
setParamswithDifficulty(string difficulty, double& searchTime, Depth& searchDepth)
{
  if (difficulty == "beginner") {
    searchTime = 0.001;
    searchDepth = 1;
  } else if (difficulty == "easy") {
    searchTime = 0.005;
    searchDepth = 3;
  } else if (difficulty == "medium") {
    searchTime = 0.1;
    searchDepth = 5;
  } else if (difficulty == "hard") {
    searchTime = 1;
    searchDepth = 8;
  } else if (difficulty == "expert") {
    searchTime = 1.5;
    searchDepth = MAX_DEPTH;
  }
}

static void
directSearch(const vector<string> &args)
{
  // elsa [depth <depth>] [fen <fen>] [debug] [time <search_time>] go

  const string fen = utils::getFen(args, START_FEN);
  const double searchTime = utils::getTime(args, DEFAULT_SEARCH_TIME);
  const Depth searchDepth = utils::getDepth(args, MAX_DEPTH);

  ChessBoard pos(fen);
  search(pos, searchDepth, searchTime, std::cout, true);
}

static void
bestMoveSearch(const vector<string> &args)
{
  // elsa bestmove [fen <fen>] [difficulty <difficulty>] [depth <depth>] [time <search_time>]

  const string fen = utils::getFen(args, START_FEN);
  const string difficulty = utils::getDifficulty(args, "expert");
  Depth depth = utils::getDepth(args, MAX_DEPTH);
  double searchTime = utils::getTime(args, DEFAULT_SEARCH_TIME);

  setParamswithDifficulty(difficulty, searchTime, depth);

  ChessBoard pos(fen);
  search(pos, depth, searchTime, std::cout, false);
  const auto bestMove = info.lastIterationResult();
  cout << printMove(bestMove.first, pos) << endl;

  pos.makeMove(bestMove.first);
  cout << pos.fen() << endl;
}

static void
nodeCount(const vector<string> &args)
{
  // elsa [depth <depth>] [fen <fen>] count

  const string fen = utils::getFen(args, START_FEN);
  const Depth depth = utils::getDepth(args, 6);

  ChessBoard pos(fen);

  cout << "Fen = " << fen << '\n';
  cout << "Depth = " << depth << "\n" << endl;

  const auto &[nodes, t] = perf::run_algo(bulkCount, pos, depth);

  cout << "Nodes(single-thread) = " << nodes << '\n';
  cout << "Time (single-thread) = " << t << " sec.\n";
  cout << "Speed(single-thread) = " << static_cast<double>(nodes) / (t * 1e6)
        << " M nodes/sec.\n" << endl;
}

static void
debugMoveGenerator(const vector<string> &args)
{
  // elsa [fen <fen>] [depth <depth>] [output <filename>] [movegen]

  const auto moveName = [] (int move)
  {
    int ip = (move & 63);
    int fp = (move >> 6) & 63;

    int ix = ip & 7, iy = (ip - ix) >> 3;
    int fx = fp & 7, fy = (fp - fx) >> 3;

    char a1 = static_cast<char>(97 + ix);             // 'a' + ix
    char a2 = static_cast<char>(49 + iy);             // '1' + iy
    char b1 = static_cast<char>(97 + fx);             // 'a' + fx
    char b2 = static_cast<char>(49 + fy);             // '1' + fy

    return string({a1, a2, b1, b2});
  };

  const string fen = utils::getFen(args, START_FEN);
  const Depth depth = utils::getDepth(args, 2);
  const string outputFile = utils::getOutputFile(args, "inp.txt");
  
  std::ofstream out(outputFile);
  ChessBoard pos(fen);
  MoveList myMoves = generateMoves(pos);
  MoveArray movesArray;
  myMoves.getMoves(pos, movesArray);

  for (const auto move : movesArray)
  {
    pos.makeMove(move);
    const auto current = bulkCount(pos, depth - 1);
    out << moveName(move) << " : " << current << '\n';
    pos.unmakeMove();
  }

  out.close();
}

static void
staticEval(const vector<string>& args)
{
  // elsa fen <fen> static

  const string fen = utils::getFen(args, START_FEN);
  ChessBoard pos(fen);

  MoveList myMoves = generateMoves(pos);
  MoveArray movesArray;
  myMoves.getMoves(pos, movesArray);
  orderMoves(pos, movesArray, MType::CAPTURES, 0);
  printMovelist(movesArray, pos);

  Score eval = evaluate<true>(pos);
  cout << "Score = " << eval << endl;
}

static void
readyOk()
{
  // Argument : elsa readyOk
  ChessBoard pos(START_FEN);

  if (bulkCount(pos, 3) == 8902)
    puts("Ready Ok!");
  else
    puts("Ready Not Ok!");
}

void
task(const vector<string>& args)
{
  if (args.empty())
  {
    puts("No Task Found!");
    puts("Type : \'elsa help\' to view command list.\n");
    return;
  }

  // Command map that associates commands with their handler functions
  const std::unordered_map<string, std::function<void(const vector<string>&)>> commandMap = {
    {"help",     [](const auto&){ helper(); }},
    {"accuracy", [](const auto&){ accuracyTest(); }},
    {"speed",    [](const auto&){ speedTest(); }},
    {"go",       [](const auto& arguments){ directSearch(arguments); }},
    {"count",    [](const auto& arguments){ nodeCount(arguments); }},
    {"movegen",  [](const auto& arguments){ debugMoveGenerator(arguments); }},
    {"static",   [](const auto& arguments){ staticEval(arguments); }},
    {"bestmove",  [](const auto& arguments){ bestMoveSearch(arguments); }},
    {"hashlegal", [](const auto&){ hashLegalTest(); }},
    {"readyOk",   [](const auto&){ readyOk(); }}
  };

  // Search for any command in the args
  string foundCommand;
  for (const auto& arg : args) {
    if (commandMap.find(arg) != commandMap.end()) {
      foundCommand = arg;
      break;
    }
  }

  if (!foundCommand.empty()) {
    const auto it = commandMap.find(foundCommand);
    it->second(args);
  } else {
    puts("No Valid Task!");
    puts("Type : \'elsa help\' to view command list.\n");
  }
}

