#include "task.h"


//* TODO Search
// Prioritise Passed Pawns in Move Reordering ("5k2/R7/2p2N2/2P3P1/p7/3p2KP/P7/1r6 w - - 0 42")
// Delta Pruning
// Check depth += 2 in Search
// Alpha-Beta lmr code.
// Test LMR condition is_passed_pawn(), castling

//* TODO EndGame
// Endgame tests fen
// Add insufficient material draw(Two minor piece vs one minor piece is a drawish)
// (Bishop | Knight vs Pawn) if the piece can capture the pawn
// (Queen vs B/R/N) Endgame
// Pawn Race (8/5ppk/7p/8/P7/8/1K6/8 w - - 0 1)

//* TODO Evaluation
// Fix AttackStrength (Should depend on opponent king safety) (rr3k2/ppq3pQ/2nb4/3p1RP1/P2P4/1N5P/1P4K1/R1B5 b - - 0 24)

//* TODO Search Extension
// If promotion is possible and no enemy piece can capture it. (3k4/8/5N2/nR6/3P4/p7/1p3PP1/6K1 w - - 0 51)
// instead move_gives_check in LMR, use it as in SearchExtension.
// Add depth 1, if opponent pawn attacks own piece
// 8/pb1n3k/1p4pp/8/2q1PQ2/Pn3P1B/1B1r2PP/2R3K1 b - - 3 31      Critical-leaf-Node extension
// 8/2K5/8/8/P3kp2/8/8/8 b - - 0 74                             (Pawn Race fix)


void test()
{
  perf::Timer T("Movegen");

  ChessBoard pos[4] = {
    ChessBoard("r2q3r/1b1k1pbp/p4np1/2BP1pN1/p1B5/P1Q5/1PP3PP/R3K2R w KQ - 0 19"),
    ChessBoard("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"),
    ChessBoard("r3k2r/1bp2pP1/5n2/1P1Q4/1pPq4/5N2/1B1P2p1/R3K2R b KQkq c3 0 1"),
    ChessBoard("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8")
  };

  int result = 0;
  for (int i = 0; i < 1000000; i++)
  {
    MoveList2 myMoves = GenerateMoves(pos[i & 3]);
    result += myMoves.checkers;
  }

  cout << "Result = " << result << endl;
}


int main(int argc, char **argv)
{
  FAST_IO();
  Init();
  Task(argc, argv);

  test();
}


/*
  r1bq1rk1/p4ppp/2Bb1n2/3p2B1/8/2N5/PPP2PPP/R2Q1RK1 b - - 0 12             // Bxh2+
  r2q3r/1b1k1pbp/p4np1/2BP1pN1/p1B5/P1Q5/1PP3PP/R3K2R w KQ - 0 19          // O-O O-O-O Qb4
  2k1r3/p1p2p1p/2p3p1/2bb1qB1/2P5/PP3P2/4N1PP/R2QK2R w KQ - 1 18           // Qd2 cxd5 Bh4
  rn1qkbnr/4p1pp/2p2p2/pp2PbN1/2pP4/2N5/PP3PPP/R1BQKB1R w KQkq - 0 8       // Qf3 g4 Nf3
  rnb1kb1r/ppp2ppp/5n2/q7/5B2/2N5/PPP1NPPP/R2QKB1R w KQkq - 4 8            // Nd4 a3 Qd3
  2kr1b1r/pppn1ppp/4bn2/q7/3Q1B2/2N5/PPP1NPPP/1K1R1B1R w - - 10 11         // f3 Qa4
  7R/1p1k2r1/3n4/p2p2B1/P2P2K1/6P1/8/8 w - - 15 49                         // Kf4
  2r1r1k1/pb1nqppp/1p6/3p4/1b2n3/PPN1P1PB/1BQ1NP1P/3RR1K1 b - - 0 1        // Nxf2
  2kr2nr/1pp1bppp/p1p2q2/4p3/P3P1b1/2NP1N2/1PP2PPP/R1BQ1RK1 w - - 0 1      // h3 Qe2              (horizon effect)
  r5k1/1p1n1ppp/pqr1p3/3p4/3PnB2/P1P1QN2/1P3PPP/2KRR3 w - - 3 16           // Nd2                 (horizon effect)
  2kr3r/pppn1ppp/4bn2/q1b5/5B2/2N3Q1/PPP1NPPP/1K1R1B1R b - - 13 12         // Ne4 g5              (horizon effect)
  3r2k1/6pp/5p2/2q5/p1P1PP2/N1Pr3P/R3Q2K/1R6 w - - 5 30                    // Rb8 e5 Rb5          (horizon effect)
  r1b2r2/pp1p2kp/2q1p1p1/2p2n2/N1B5/2PP4/PP3PPP/1R1QR1K1 w - - 1 18        // d4                  (horizon effect)
  5rk1/1pnqb1r1/p2pRp2/2pP1PpQ/P1P1NpP1/3B1P2/1P1K4/7R w - - 0 1           // Nxg5                (LMR Test)
  7k/5R2/4p1r1/p2pP3/P2P4/1P1q4/5Q2/4K3 b - - 4 57                         // Qe4+                (LMR Test)
  r4rk1/1p1b1p2/2pp1bp1/p6p/2PN4/1P2P1P1/P4PBP/3R1RK1 w - - 0 20           // D.M (Nxc6)
  r1b2Q2/1pq2p2/p1kppNp1/P1p5/R3P3/2Nn4/1nK2PPP/7R w - - 3 22              // Nfd5
  r1bq1rk1/pppnnppp/4p3/3pP3/1b1P4/2NB3N/PPP2PPP/R1BQK2R w KQ - 3 7        // Bxh7+
  2rq4/p6k/1p1p2pp/2pP1p2/1bP2R2/1R4QP/P1B2PPK/4r3 w - - 0 1               // Rxf5
  r1bqr1k1/pp1nbppp/2pp1n2/4p3/2B1P3/2PP1N2/PP1N1PPP/R1BQR1K1 w - - 1 9    // Bxf7+
  r2r2k1/pp1bbp1p/2n3pB/q2p2N1/8/P1PB4/5PPP/2QRR1K1 w - - 0 1              // Rxe7
  r4rk1/ppqb2p1/2nbp3/3p2PQ/P2P1P2/1N1B2pP/1P6/R1B2RK1 b - - 2 19          // B.M Rf5

  -----------------------------------------------------------------------

  8/5k2/R5p1/p7/1r5P/4BK2/8/8 w - - 8 45                                   // D.M (Rxa5)          (Endgame)
  rB6/P6p/2k3p1/R6P/6b1/4K3/8/8 w - - 1 36                                 // h6                  (Endgame)
  8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1                                  // Kb1
  8/5Pk1/4K2p/4N1PP/8/8/5r2/8 b - - 2 60                                   // B.M Rxf7
  2kr3r/pppq4/3b4/3bn3/2N1p1p1/PP2P1N1/1B3PPP/2RQ1RK1 b - - 1 18           // B.M Nf3+

  Quiescence Explosion:
  q2k2q1/2nqn2b/1n1P1n1b/2rnr2Q/1NQ1QN1Q/3Q3B/2RQR2B/Q2K2Q1 w - - 0 1
*/


/*
  8/5P2/1p6/5k1p/1n1KN3/6P1/8/4r3 b - - 0 55              B.M Rd1+
  Extend Search if a pawn can promote and can't be captured.

  rr4k1/1p3p2/p3pB1Q/3pP3/b1pP2PK/5P2/2P1n2P/4R3 b - - 0 27
  r3k2r/1B3p2/p3p3/8/Pp1q4/1P6/R1QN1P1b/5R1K b kq - 2 23
  4rrk1/3R1p2/p2Q2p1/q7/5B2/5b1P/1P3P1P/5RK1 b - - 3 26
  r3r1k1/1p3p2/p3p1n1/q2pP1BP/b1pP2P1/2P2Q2/2P2P2/R4RK1 b - - 0 21
  King Safety Understanding | King Mobility | Distance of pieces from the king

  3q3k/Pp3p1b/2p1p3/2PpP3/P7/2P3r1/3B4/R4QK1 w - - 0 26
  Fix Pawn Structure

  8/8/8/6k1/1B2Kp2/8/2b1p3/8 w - - 2 84
  No clue whats fucking things up?

  B7/7k/8/6KP/8/8/8/8 w - - 1 156
  Attention in regards to LoneKing function.


  8/8/8/8/4B2k/5K2/4B3/8 w - - 45 207
  2 same-color bishops is a theoretical draw.

  1r1q1rk1/3bppbp/p4np1/2pPp3/2P1P3/2N1QP2/P3N1PP/2KR1B1R b - - 0 15
  Excellent position to illustrate open_files and KingSafety in long term prospect.


  8/8/6kP/8/5P2/8/4K3/8 w - - 1 62
  Avoid the use of LoneKing in these cases.

  8/8/6kP/1p3N2/1npKB3/8/8/8 w - - 5 55
  2 minor piece vs 1 minor piece is draw

  1k6/1P1R3p/6pr/5p2/5P1P/4K1P1/8/8 w - - 35 78
  Bring the king closer to pawn which is closer to promotion

  8/8/6p1/2k1K3/4N3/8/8/8 b - - 3 89
  KPNK is draw if knight can capture the pawn and there is no captures in position

  8/8/8/5b2/2Pk4/1P6/P4K2/8 w - - 3 62
  No winning with only a minor piece

  8/3P3k/5Kp1/2R4p/8/8/3r4/8 w - - 1 52     D.M Ke7

  r7/P7/R2K4/5pk1/6p1/6P1/8/8 w - - 8 51    D.M Kc7

  8/3k4/R5P1/1r2K3/8/8/8/8 w - - 4 69       B.M Kf6
*/

/*
  endgame-suite

  8/6k1/1n1p2p1/3P1pPp/7P/1B1NK3/8/8 b - - 35 84

  8/8/6p1/2k1K3/4N3/8/8/8 b - - 3 89

  5K2/6Pk/8/8/3P4/1b6/8/4B3 b - - 13 70

  8/4k3/6pp/8/8/P3P3/4K3/8 w - - 0 44

  r7/P7/R2K4/5pk1/6p1/6P1/8/8 w - - 8 51

  8/3k4/R5P1/1r2K3/8/8/8/8 w - - 4 69

  8/8/r7/1R6/4k1P1/6K1/8/8 w - - 11 68

  8/8/r7/4k1P1/1R6/6K1/8/8 b - - 0 69
*/
