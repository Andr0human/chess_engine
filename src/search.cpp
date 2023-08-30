
#include "search.h"

Move pvArray[(MAX_PLY * MAX_PLY + MAX_PLY) / 2];
Move thread_array[MAX_THREADS][(MAX_PLY * MAX_PLY) / 2];
const string StartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

#ifndef TOOLS


void
movcpy(Move* pTarget, const Move* pSource, int n)
{ while (n-- && (*pTarget++ = *pSource++)); }


void
reset_pv_line()
{
    for (int i = 0; i < 800; i++)
        pvArray[i] = 0;
}

Score
checkmate_score(int ply)
{ return -VALUE_MATE + (20 * ply); }

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
createMoveOrderList(ChessBoard& pos)
{
    /* Stores time used to evaluate each root move.
    Useful in ordering root moves in iterative search. */

    // res -> -1(Lost), -2(Draw), -3(Invalid), >1(Zero-depth Move)

    MoveList movelist = generate_moves(pos);

    // To ensure, the zero move is best_move.
    order_generated_moves(movelist, false);
    moc.initialise(movelist);

    return (movelist.size() > 0) ?
           (*movelist.begin()) : (pos.KA > 0 ? -1 : -2);
}

bool
is_valid_move(Move move, ChessBoard pos)
{
    // Get Init. and Dest. Square from encoded move.
    Square ip = move & 63, vMove/* , fp = (move >> 6) & 63 */;

    // No piece on initial square
    if (pos.piece_on_square(ip) == NO_PIECE) return false;

    // Piece of same colour to move
    // if (_cb.pColor * _cb.Pieces[ip] < 0) return false;

    // No same colour for Initial and Dest Sq. Piece
    // if (_cb.pColor * _cb.Pieces[fp] > 0) return false;

    // Generate all legal moves for curr. Position
    MoveList myMoves = generate_moves(pos);

    // Take only Init. and Dest. Sq. to Compare(Filter everything else.)
    // Match with all generated legal moves, it found return valid, else not-valid.

    move &= (1 << 12) - 1;
    for (const Move vmove : myMoves)
    {
        vMove = vmove & ((1 << 12) - 1);
        // Valid Move Found.
        if (move == vMove) return true;
    }
    // Move not matched with any generated legal moves, thus invalid.
    return false;
}


#endif

#ifndef SEARCH_UTIL


bool
ok_to_do_LMR(Depth depth, MoveList& myMoves)
{
    if (depth < 3) return false;
    // if (myMoves.KingAttackers || myMoves.__count < 6) return false;
    return true;
}

int
root_reduction(Depth depth, int num)
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
reduction (Depth depth, int move_no)
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
MaterialCount(ChessBoard& pos)
{
    int answer = 0;

    answer += 100 * popcount((pos.piece(WHITE, PAWN  )) | pos.piece(BLACK, PAWN  ));
    answer += 300 * popcount((pos.piece(WHITE, BISHOP)) | pos.piece(BLACK, BISHOP));
    answer += 300 * popcount((pos.piece(WHITE, KNIGHT)) | pos.piece(BLACK, KNIGHT));
    answer += 500 * popcount((pos.piece(WHITE, ROOK  )) | pos.piece(BLACK, ROOK  ));
    answer += 900 * popcount((pos.piece(WHITE, QUEEN )) | pos.piece(BLACK, QUEEN ));
    return answer;
}

#endif

#ifndef SEARCH_COMMON

Score
QuieSearch(ChessBoard& pos, Score alpha, Score beta, int ply, int __dol)
{    
    // Check if Time Left for Search
    if (info.time_over())
        return TIMEOUT;


    if (has_legal_moves(pos) == false)
    {
        Score score = pos.king_in_check() ? checkmate_score(ply) : VALUE_ZERO;
        pos.remove_movegen_extra_data();
        return score;
    }

    // if (ka_pieces.attackers) return AlphaBeta_noPV(_cb, 1, alpha, beta, ply);

    // Get a 'Stand Pat' Score
    Score stand_pat = Evaluate(pos);

    // Checking for beta-cutoff
    if (stand_pat >= beta)
    {
        // Usually called at the end of move-generation.
        pos.remove_movegen_extra_data();
        return beta;
    }

    // int BIG_DELTA = 925;
    // if (stand_pat < alpha - BIG_DELTA) return alpha;

    if (stand_pat > alpha) alpha = stand_pat;

    MoveList myMoves = generate_moves(pos, true);
    order_generated_moves(myMoves, false);

    for (const Move move : myMoves)
    {
        // Check based on priority for captures & checks
        int move_priority = (move >> 21) & 31;

        if (move_priority <= 10)
            continue;

        pos.MakeMove(move);
        Score score = -QuieSearch(pos, -beta, -alpha, ply + 1, __dol + 1);
        pos.UnmakeMove();

        if (std::abs(score) == TIMEOUT)
            return TIMEOUT;

        // Check for Beta-cutoff
        if (score >= beta) return beta;

        // better move found!
        if (score > alpha) alpha = score;
    }

    return alpha;
}


Score
AlphaBeta_noPV(ChessBoard& _cb, Depth depth, Score alpha, Score beta, int ply)
{
    if (has_legal_moves(_cb) == false)
        return _cb.king_in_check() ? checkmate_score(ply) : 0;

    if (depth <= 0)
        return QuieSearch(_cb, alpha, beta, ply, 0);

    auto myMoves = generate_moves(_cb);
    order_generated_moves(myMoves, false);

    for (const Move move : myMoves)
    {
        _cb.MakeMove(move);
        Score eval = -AlphaBeta_noPV(_cb, depth - 1, -beta, -alpha, ply + 1);
        _cb.UnmakeMove();
        
        if (eval > alpha)
            alpha = eval;
        if (eval >= beta)
            return beta;
    }
    
    return alpha;
}

#endif
