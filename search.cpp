

#include "search.h"
#include <iomanip>


std::mutex mute;
bool search_time_left, extra_time_left, perf_test = false;
double alloted_search_time = 2, alloted_extra_time = 0;
uint64_t nodes_hits = 0, qnodes_hits = 0;
int threadCount = 4;
const double default_allocate_time = 2.0;
int pvArray[(maxPly * maxPly + maxPly) / 2];
int thread_array[maxThreadCount][(maxPly * maxPly) / 2];
const string default_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

int64_t total_can_pos = 0;
int64_t this_can_pos = 0;

#ifndef TOOLS

void timer() {
    /* int _x = alloted_search_time * 1000, _y = alloted_extra_time * 1000;
    std::this_thread::sleep_for(std::chrono::milliseconds(_x)); */
    const auto _x = static_cast<uint64_t>(alloted_search_time * 1e6);
    std::this_thread::sleep_for(std::chrono::microseconds(_x));

    search_time_left = extra_time_left = false;
    /* if (info.use_verification_Search()) {
        search_time_left = true;
    } else {
        _y = 0;
        search_time_left = extra_time_left = false;
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(_y));
    extra_time_left = false; */
}

void timer2() {
    const auto _x = static_cast<uint64_t>(alloted_search_time * 1000);
    std::this_thread::sleep_for(std::chrono::microseconds(_x * 1000));
    search_time_left = false;
    return;
}

void movcpy (int* pTarget, const int* pSource, int n) {
    while (n-- && (*pTarget++ = *pSource++));
}

void PRINT_LINE(chessBoard board, int line[], int __N) {

    for (int i = 0; i < __N; i++) {
        if (!line[i]) break;
        cout << print(line[i], board) << " ";
        board.MakeMove(line[i]);
    }
    cout << endl;
}

void PRINT_LINE(chessBoard _cb, std::vector<int> line) {

    for (const auto move : line) {
        if (move == 0) break;
        cout << print(move, _cb) << " ";
        _cb.MakeMove(move);
    }
    cout << endl;
}

void pre_status(int __dep, int __cnt) {
    nodes_hits = qnodes_hits = 0;
    if (!perf_test) {
        cout << "Looking at Depth : " << __dep << endl;
        cout << "ValWindow => " << (valWindow << __cnt) << endl;
    }
}

void post_status(chessBoard &_cb, int _m, int _e, perf_clock start_time) {

    perf_time x = perf::now() - start_time;
    const auto eval = _e * (2 * _cb.color - 1);
    
    if (!perf_test) {
        cout << "Node Hits :  " << nodes_hits << " Nodes. | qNode Hits : " << qnodes_hits << " Nodes.\n";
        cout << "Eval : " << static_cast<float>(eval) / 100.0 << '\n';
        cout << "Best Move : " << print(_m, _cb) << '\n';
        cout << "Time Used : " << x.count() << "\n\n" << std::flush;
    }
}

void curr_depth_status(chessBoard &_cb) {

    cout << std::setprecision(2);
    cout << "Depth ";
    if (info.last_depth() < 10) cout << " ";

    cout << info.last_depth() << " | Eval : " << info.last_eval() << "\t| PV : ";
    cout << std::setprecision(3);
    PRINT_LINE(_cb, info.pvAlpha, info.last_depth());
}

void Show_Searched_Info(chessBoard &_cb) {
    cout << "Depth Searched : " << info.last_depth() << endl;
    // cout << "Max Depth Reached : " << info.max_ply << endl;
    // cout << "Max Qs Depth Reached : " << info.max_qs_ply << endl;
    cout << "Best Move : " << print(info.best_move(), _cb) << endl;
    cout << "Eval : " << info.last_eval() << endl;
    cout << "LINE : ";
    PRINT_LINE(_cb, info.pvAlpha, info.last_depth());
}

void reset_pv_line() {
    for (int i = 0; i < 800; i++)
        pvArray[i] = 0;
}

int checkmate_score(int ply) {
    return (negInf >> 1) + (20 * ply);
}

#endif

#ifndef MOVE_GENERATION

void order_generated_moves(MoveList& myMoves, bool pv_moves) {

    /* We order all legal moves in current position based on their type.
    e.g - Cap. Moves, PV Moves */

    uint64_t start = 0, __n = myMoves.size(), l2;

    // Put all PV moves before rest of the legal moves
    if (pv_moves) {
        for (uint64_t i = 0; i < __n; i++)
            if (info.is_part_of_pv(myMoves.pMoves[i])) 
                std::swap(myMoves.pMoves[i], myMoves.pMoves[start++]);
    }

    // Put all Capture moves before rest of the legal moves
    for (uint64_t i = start; i < __n; i++)
        if ((myMoves.pMoves[i] >> 21) > 10)
            std::swap(myMoves.pMoves[i], myMoves.pMoves[start++]);

    // Order all pv and capture moves based on their move priority
    l2 = start;
    for (uint64_t i = 0; i < l2; i++)
        for (uint64_t j = i + 1; j < l2; j++)
            if (myMoves.pMoves[j] > myMoves.pMoves[i])
                std::swap(myMoves.pMoves[i], myMoves.pMoves[j]);

}

int createMoveOrderList(chessBoard &_cb) {

    /* Stores time used to evaluate each root move.
    Useful in ordering root moves in iterative search. */

    // res -> -1(Lost), -2(Draw), -3(Invalid), >1(Zero-depth Move)

    MoveList tmp = generate_moves(_cb);
    order_generated_moves(tmp, false);
    moc.initialise(tmp);
    moc.reset();

    int res = 0;

    if (tmp.size()) {
        res = tmp.pMoves[0];
    } else {
        res = _cb.KA > 0 ? -1 : -2;
    }

    return res;
}

bool is_valid_move(int move, chessBoard _cb) {

    int ip = move & 63, vMove/* , fp = (move >> 6) & 63 */;                      // Get Init. and Dest. Square from encoded move.

    if (!_cb.board[ip]) return false;                               // No piece on initial square
    // if (_cb.pColor * _cb.Pieces[ip] < 0) return false;              // Piece of same colour to move
    // if (_cb.pColor * _cb.Pieces[fp] > 0) return false;              // No same colour for Initial and Dest Sq. Piece

    MoveList myMoves = generate_moves(_cb);                                      // Generate all legal moves for curr. Position

    // Take only Init. and Dest. Sq. to Compare(Filter everything else.)
    // Match with all generated legal moves, it found return valid, else not-valid.

    move &= (1 << 12) - 1;

    for (const auto vmove : myMoves) {
        vMove = vmove & ((1 << 12) - 1);
        if (move == vMove) return true;                             // Valid Move Found.
    }
    return false;                                                   // Move not matched with any generated legal moves, thus invalid.
}


#endif

#ifndef SEARCH_UTIL

bool ok_to_do_nullmove(chessBoard& _cb) {
    // if (ka_pieces.attackers) return false;
    if (info.side2move != _cb.color) return false;
    return true;
}

bool ok_to_fprune(int depth, chessBoard& _cb, MoveList& myMoves, int beta) {
    if (_cb.king_in_check()) return false;
    int margin = 250;
    if (depth == 2) margin = 420;
    int s_eval = ev.Evaluate(_cb);
    if (s_eval + margin < beta) return true;
    return false;
}

int apply_search_extensions(MoveList& myMoves) {
    int R = 0;
    // if (myMoves.KingAttackers) R++;
    
    return R;
}

bool ok_to_do_LMR(int depth, MoveList& myMoves) {
    if (depth < 3) return false;
    // if (myMoves.KingAttackers || myMoves.__count < 6) return false;
    return true;
}

int root_reduction(int depth, int num) {
    if (depth < 3) return 0;
    if (depth < 6) {
        if (num < 9) return 1;
        // if (num < 12) return 2;
        return 2;
    }
    if (num < 8) return 2;
    // if (num < 15) return 3;
    return 3;
}

int reduction (int depth, int move_no) {

    if (depth < 2) return 0;
    if (depth < 4 && move_no > 9) return 1; 

    if (depth < 7) {
        if (move_no < 9) return 1;
        return 2;
    }

    if (move_no < 12) return 1;
    if (move_no < 24) return 2;
    return 3;
}

int MaterialCount(chessBoard& _cb) {
    int answer = 0;
    answer += 100 * (__ppcnt(PAWN(WHITE))   + __ppcnt(PAWN(BLACK)));
    answer += 300 * (__ppcnt(BISHOP(WHITE)) + __ppcnt(BISHOP(BLACK)));
    answer += 300 * (__ppcnt(KNIGHT(WHITE)) + __ppcnt(KNIGHT(BLACK)));
    answer += 500 * (__ppcnt(ROOK(WHITE))   + __ppcnt(ROOK(BLACK)));
    answer += 900 * (__ppcnt(QUEEN(WHITE))  + __ppcnt(QUEEN(BLACK)));
    return answer;
}

#endif

#ifndef SEARCH_COMMON

int QuieSearch(chessBoard &_cb, int alpha, int beta, int ply, int __dol) {
    
    if (!extra_time_left) return valUNKNOWN;                                // Check if Time Left for Search
    if (__dol > info.max_qs_ply) info.max_qs_ply = __dol;
    qnodes_hits++;
    

    if (has_legal_moves(_cb) == false)
        return _cb.king_in_check() ? checkmate_score(ply) : 0;

    // if (ka_pieces.attackers) return AlphaBeta_noPV(_cb, 1, alpha, beta, ply);

    int stand_pat = ev.Evaluate(_cb);                                       // Get a 'Stand Pat' Score
    if (stand_pat >= beta) return beta;                                     // Checking for beta-cutoff

    // int BIG_DELTA = 925;
    // if (stand_pat < alpha - BIG_DELTA) return alpha;

    if (stand_pat > alpha) alpha = stand_pat;

    auto myMoves = generate_moves(_cb, true);
    order_generated_moves(myMoves, false);


    for (const auto move : myMoves) {
        int move_priority = (move >> 21) & 31;
        if (move_priority > 10) {                                           // Check based on priority for captures & checks
            _cb.MakeMove(move);                                             // Make current move
            int score = -QuieSearch(_cb, -beta, -alpha, ply + 1, __dol + 1);
            _cb.UnmakeMove();                                               // Takeback made move
            if (__abs(score) == valUNKNOWN) return valUNKNOWN;
            if (score >= beta) return beta;                                 // Check for Beta-cutoff
            if (score > alpha) alpha = score;                               // Check if a better move is found
        }
    }
    return alpha;
}

int AlphaBeta_noPV(chessBoard &_cb, int depth, int alpha, int beta, int ply) {
    // if (!extra_time_left) return valUNKNOWN;
    nodes_hits++;
    if (has_legal_moves(_cb) == false)
        return _cb.king_in_check() ? checkmate_score(ply) : 0;

    if (depth <= 0) return QuieSearch(_cb, alpha, beta, ply, 0);

    auto myMoves = generate_moves(_cb);
    order_generated_moves(myMoves, false);

    for (const auto move : myMoves) {

        _cb.MakeMove(move);
        int eval = -AlphaBeta_noPV(_cb, depth - 1, -beta, -alpha, ply + 1);
        _cb.UnmakeMove();
        
        if (eval > alpha) alpha = eval;
        if (eval >= beta) return beta;
    }
    
    return alpha;
}

#endif
