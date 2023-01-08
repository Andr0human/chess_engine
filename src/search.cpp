
#include "search.h"

MoveType pvArray[(maxPly * maxPly + maxPly) / 2];
MoveType thread_array[maxThreadCount][(maxPly * maxPly) / 2];
const string startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

#ifndef TOOLS


void
movcpy(MoveType* pTarget, const MoveType* pSource, int n)
{ while (n-- && (*pTarget++ = *pSource++)); }


void
reset_pv_line()
{
    for (int i = 0; i < 800; i++)
        pvArray[i] = 0;
}

int
checkmate_score(int ply)
{ return (negInf >> 1) + (20 * ply); }

#endif

#ifndef MOVE_GENERATION

void
order_generated_moves(MoveList& myMoves, bool pv_moves)
{
    /* We order all legal moves in current position based on their type.
    e.g - Cap. Moves, PV Moves */

    uint64_t start = 0, __n = myMoves.size(), l2;

    // Put all PV moves before rest of the legal moves
    if (pv_moves)
    {
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
    {
        for (uint64_t j = i + 1; j < l2; j++)
        {
            if (myMoves.pMoves[j] > myMoves.pMoves[i])
                std::swap(myMoves.pMoves[i], myMoves.pMoves[j]);
        }
    }
}

int
createMoveOrderList(chessBoard& _cb)
{
    /* Stores time used to evaluate each root move.
    Useful in ordering root moves in iterative search. */

    // res -> -1(Lost), -2(Draw), -3(Invalid), >1(Zero-depth Move)

    MoveList movelist = generate_moves(_cb);

    // To ensure, the zero move is best_move.
    order_generated_moves(movelist, false);
    moc.initialise(movelist);

    return (movelist.size() > 0) ?
           (*movelist.begin()) : (_cb.KA > 0 ? -1 : -2);
}

bool
is_valid_move(MoveType move, chessBoard _cb)
{
    int ip = move & 63, vMove/* , fp = (move >> 6) & 63 */;                      // Get Init. and Dest. Square from encoded move.

    if (!_cb.board[ip]) return false;                               // No piece on initial square
    // if (_cb.pColor * _cb.Pieces[ip] < 0) return false;              // Piece of same colour to move
    // if (_cb.pColor * _cb.Pieces[fp] > 0) return false;              // No same colour for Initial and Dest Sq. Piece

    MoveList myMoves = generate_moves(_cb);                                      // Generate all legal moves for curr. Position

    // Take only Init. and Dest. Sq. to Compare(Filter everything else.)
    // Match with all generated legal moves, it found return valid, else not-valid.

    move &= (1 << 12) - 1;
    for (const auto vmove : myMoves)
    {
        vMove = vmove & ((1 << 12) - 1);
        if (move == vMove) return true;                             // Valid Move Found.
    }
    return false;                                                   // Move not matched with any generated legal moves, thus invalid.
}


#endif

#ifndef SEARCH_UTIL


bool
ok_to_do_LMR(int depth, MoveList& myMoves)
{
    if (depth < 3) return false;
    // if (myMoves.KingAttackers || myMoves.__count < 6) return false;
    return true;
}

int
root_reduction(int depth, int num)
{
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

int
reduction (int depth, int move_no)
{
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

int
MaterialCount(chessBoard& _cb)
{
    int answer = 0;
    answer += 100 * (__ppcnt (PAWN(WHITE))  + __ppcnt(PAWN(BLACK)));
    answer += 300 * (__ppcnt(BISHOP(WHITE)) + __ppcnt(BISHOP(BLACK)));
    answer += 300 * (__ppcnt(KNIGHT(WHITE)) + __ppcnt(KNIGHT(BLACK)));
    answer += 500 * (__ppcnt(ROOK(WHITE))   + __ppcnt(ROOK(BLACK)));
    answer += 900 * (__ppcnt(QUEEN(WHITE))  + __ppcnt(QUEEN(BLACK)));
    return answer;
}

#endif

#ifndef SEARCH_COMMON

int
QuieSearch(chessBoard& _cb, int alpha, int beta, int ply, int __dol)
{    
    // Check if Time Left for Search
    if (info.time_over())
        return TIMEOUT;


    if (has_legal_moves(_cb) == false)
    {
        int result = _cb.king_in_check() ? checkmate_score(ply) : 0;
        _cb.remove_movegen_extra_data();
        return result;
    }

    // if (ka_pieces.attackers) return AlphaBeta_noPV(_cb, 1, alpha, beta, ply);

    int stand_pat = ev.Evaluate(_cb);                                       // Get a 'Stand Pat' Score
    if (stand_pat >= beta)
    {
        _cb.remove_movegen_extra_data();                 // Usually called at the end of move-generation.
        return beta;                                     // Checking for beta-cutoff
    }

    // int BIG_DELTA = 925;
    // if (stand_pat < alpha - BIG_DELTA) return alpha;

    if (stand_pat > alpha) alpha = stand_pat;

    auto myMoves = generate_moves(_cb, true);
    order_generated_moves(myMoves, false);

    for (const MoveType move : myMoves)
    {
        int move_priority = (move >> 21) & 31;
        // int cpt = ((move >> 15) & 7);
        if (move_priority > 10)
        {                                           // Check based on priority for captures & checks
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

int
AlphaBeta_noPV(chessBoard &_cb, int depth, int alpha, int beta, int ply)
{
    if (has_legal_moves(_cb) == false)
        return _cb.king_in_check() ? checkmate_score(ply) : 0;

    if (depth <= 0) return QuieSearch(_cb, alpha, beta, ply, 0);

    auto myMoves = generate_moves(_cb);
    order_generated_moves(myMoves, false);

    for (const MoveType move : myMoves)
    {
        _cb.MakeMove(move);
        int eval = -AlphaBeta_noPV(_cb, depth - 1, -beta, -alpha, ply + 1);
        _cb.UnmakeMove();
        
        if (eval > alpha) alpha = eval;
        if (eval >= beta) return beta;
    }
    
    return alpha;
}

#endif
