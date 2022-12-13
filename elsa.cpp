
#include "task.h"

#ifndef __DEBUG__

void show_counter() {
    cout << "\nCounter Status!!\n";

    cout << "This  --> " << tmp_this_counter << '\n';
    cout << "Total --> " << tmp_total_counter << '\n';

    const auto ratio = static_cast<float>(tmp_this_counter)
                     / static_cast<float>(tmp_total_counter);

    cout << "% = " << ratio * 100.0 << "%. \n";

}

void movegen_test() {

    chessBoard _cb = string("r2q3r/1b1k1pbp/p4np1/2BP1pN1/p1B5/P1Q5/1PP3PP/R3K2R w KQ - 0 19");

    double total_time = 0;
    const int run_times = static_cast<int>(5e6);

    for (int runs = 0; runs < 10; runs++) {

        const auto start = perf::now();

        for (int i = 0; i < run_times; i++) {
            king_attackers(_cb);
            _cb.Pieces[8] = 0;
        }

        const auto end  = perf::now();
        perf_ms_time duration = end - start;
        total_time += duration.count();
        cout << duration.count() << " ms." << endl;
    }

    const auto avg_time = total_time / 10.0;
    cout << "Avg : " << avg_time << " ms." << endl;
}

#endif

void test() {
    chessBoard __pos1 = default_fen;
    chessBoard __pos2 = default_fen;

    const int mv1 = 1054476;
    const int mv2 = 6452;
    const int mv3 = 1062214;
    const int mv4 = 15033;

    __pos1.MakeMove(mv1);
    __pos1.MakeMove(mv2);
    __pos1.MakeMove(mv3);
    __pos1.MakeMove(mv4);

    cout << "Var1 = " << __pos1.Hash_Value << endl;

    __pos2.MakeMove(mv1);
    __pos2.MakeMove(mv4);
    __pos2.MakeMove(mv3);
    __pos2.MakeMove(mv2);

    cout << "Var2 = " << __pos2.Hash_Value << endl;
}

int main(int argc, char **argv)
{
    FAST_IO();
    init();
    Task(argc, argv);

    // std::cout << "Start !" << endl;

    // test();

    // chessBoard _cb = default_fen;
    chessBoard _cb = string("r1bqkb1r/pppppBpp/2n2n2/8/4P3/8/PPPP1PPP/RNBQK1NR b KQkq - 0 3");
    // _cb.show();

    // cout << "Board Initialised!!\n\n" << endl;

    cout << "Start = " << _cb.Hash_Value << endl;


    // int moves[] = {1055784, 27898, 1076344, 27379, 27008560};

    // for (int i = 0; i < 5; i++)
    // {
    //     cout << "Move = " << print(moves[i], _cb) << endl;
    //     _cb.MakeMove(moves[i]);
    //     auto __x = _cb.generate_hashKey();
    //     std::cout << "End = " << _cb.Hash_Value << " | " << __x << endl;
    // }

    // MakeMove_Iterative(_cb, 8);

    // for (int i = 0; i < 6; i++)
    //     _cb.UnmakeMove();

    int move = 16870780;
    _cb.MakeMove(move);
    // int move2 = 62005238;
    // _cb.MakeMove(move2);

    // cout << "HashIndex - " << TT.hash_key(21) << endl;

    // _cb.show();
    // std::cout << "Done!" << endl;

    auto __x = _cb.generate_hashKey();
    std::cout << "End = " << _cb.Hash_Value << " | " << __x << endl;

    // MoveList myMoves = generate_moves(_cb);
    // cout << myMoves.size() << endl;
    // print(myMoves, _cb);
}


/*
    r1bq1rk1/p4ppp/2Bb1n2/3p2B1/8/2N5/PPP2PPP/R2Q1RK1 b - - 0 12             // Bxh2+
    r2q3r/1b1k1pbp/p4np1/2BP1pN1/p1B5/P1Q5/1PP3PP/R3K2R w KQ - 0 19          // O-O O-O-O Qb4
    2k1r3/p1p2p1p/2p3p1/2bb1qB1/2P5/PP3P2/4N1PP/R2QK2R w KQ - 1 18           // Qd2 cxd5 Bh4
    rn1qkbnr/4p1pp/2p2p2/pp2PbN1/2pP4/2N5/PP3PPP/R1BQKB1R w KQkq - 0 8       // Qf3 g4 Nf3
    rnb1kb1r/ppp2ppp/5n2/q7/5B2/2N5/PPP1NPPP/R2QKB1R w KQkq - 4 8            // Nd4 a3 Qd3
    rnbqkb1r/1p1n1ppp/p3p3/1BppP3/3P4/2N2N2/PPP2PPP/R1BQK2R w KQkq - 0 7     // Bxd7+ Bg5
    2kr1b1r/pppn1ppp/4bn2/q7/3Q1B2/2N5/PPP1NPPP/1K1R1B1R w - - 10 11         // f3 Qa4
    7R/1p1k2r1/3n4/p2p2B1/P2P2K1/6P1/8/8 w - - 15 49                         // Kf4
    2r1r1k1/pb1nqppp/1p6/3p4/1b2n3/PPN1P1PB/1BQ1NP1P/3RR1K1 b - - 0 1        // Nxf2
    2kr2nr/1pp1bppp/p1p2q2/4p3/P3P1b1/2NP1N2/1PP2PPP/R1BQ1RK1 w - - 0 1      // h3 Qe2              (horizon effect)
    r5k1/1p1n1ppp/pqr1p3/3p4/3PnB2/P1P1QN2/1P3PPP/2KRR3 w - - 3 16           // Nd2                 (horizon effect)
    2kr3r/pppn1ppp/4bn2/q1b5/5B2/2N3Q1/PPP1NPPP/1K1R1B1R b - - 13 12         // Ne4 g5              (horizon effect)
    3r2k1/6pp/5p2/2q5/p1P1PP2/N1Pr3P/R3Q2K/1R6 w - - 5 30                    // Rb8 e5 Rb5          (horizon effect)
    r1b2r2/pp1p2kp/2q1p1p1/2p2n2/N1B5/2PP4/PP3PPP/1R1QR1K1 w - - 1 18        // d4                  (horizon effect)
    5rk1/1pnqb1r1/p2pRp2/2pP1PpQ/P1P1NpP1/3B1P2/1P1K4/7R w - - 0 1           // Nxg5                (LMR Test)
    4rrk1/1p3ppp/p7/3N3n/P1PpP3/R7/5PPP/5RK1 w - - 0 20                      // f3
    r4rk1/1p1b1p2/2pp1bp1/p6p/2PN4/1P2P1P1/P4PBP/3R1RK1 w - - 0 20           // D.M (Nxc6)
    5r2/7k/1p1p1r1p/2pP1Pp1/2Pq4/PK3R1P/4Q3/5R2 w - - 1 34                   // D.M (Ka4)
    r1b2Q2/1pq2p2/p1kppNp1/P1p5/R3P3/2Nn4/1nK2PPP/7R w - - 3 22              // Nfd5
    r1bq1rk1/pppnnppp/4p3/3pP3/1b1P4/2NB3N/PPP2PPP/R1BQK2R w KQ - 3 7        // Bxh7+
    2rq4/p6k/1p1p2pp/2pP1p2/1bP2R2/1R4QP/P1B2PPK/4r3 w - - 0 1               // Rxf5
    r1bqr1k1/pp1nbppp/2pp1n2/4p3/2B1P3/2PP1N2/PP1N1PPP/R1BQR1K1 w - - 1 9    // Bxf7+
    r2r2k1/pp1bbp1p/2n3pB/q2p2N1/8/P1PB4/5PPP/2QRR1K1 w - - 0 1              // Rxe7

    -----------------------------------------------------------------------
    
    8/5k2/R5p1/p7/1r5P/4BK2/8/8 w - - 8 45                                   // D.M (Rxa5)          (Endgame)
    rB6/P6p/2k3p1/R6P/6b1/4K3/8/8 w - - 1 36                                 // h6                  (Endgame)
    8/8/4K1n1/4p1P1/4k3/8/8/8 b - - 1 63                                     // D.M (Ke3)           (Endgame)
    8/7p/6p1/pP3p2/P2p1k2/1r5P/3KBP2/8 b - - 3 44                            // D.M (Rxh3)          (Endgame)
    8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1                                  // Kb1

    Search Explosion :
    qqqqkqqq/qqqqqqqq/8/8/8/8/QQQQQQQQ/QQQQKQQQ w - - 0 1
    rrrrkrrr/rrrrrrrr/8/8/8/8/RRRRRRRR/RRRRKRRR w - - 0 1

    To check for depth 1 quiescense:
    
    5r2/pp1k4/3p2p1/4b2p/4P2Q/P5PP/1P1q1n2/4RRK1 b - - 4 38
    r2q1rk1/pp1bppbp/3p2p1/6B1/P1BQP1n1/1PN5/2P2PPP/R4RK1 w - - 1 12
    8/5ppp/4k3/p2pP3/Pr3PP1/R3K3/7P/8 b - - 0 33

*/


// To check : K5R1/5P1p/1B4P1/1k6/8/8/6r1/r7 w - - 1 2

