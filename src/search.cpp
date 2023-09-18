
#include "search.h"

Move pvArray[MAX_PV_ARRAY_SIZE];
Move thread_array[MAX_THREADS][MAX_PV_ARRAY_SIZE];
const string StartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

#ifndef TOOLS


void
movcpy(Move* pTarget, const Move* pSource, int n)
{ while (n-- && (*pTarget++ = *pSource++)); }


void
ResetPvLine()
{
    for (size_t i = 0; i < MAX_PV_ARRAY_SIZE; i++)
        pvArray[i] = NULL_MOVE;
}

Score
CheckmateScore(int ply)
{ return -VALUE_MATE + (20 * ply); }

#endif

#ifndef MOVE_GENERATION


void
OrderMoves(MoveList& myMoves, bool pv_moves, bool check_moves)
{
    const auto IsCaptureMove = [] (Move m)
    { return (m & (CAPTURES << 21)) != 0; };

    const auto IsPvMove = [&] (Move m)
    { return info.IsPartOfPV(m); };

    const auto IsCheckMove = [] (Move m)
    { return (m & (CHECK << 21)) != 0; };

    const auto PrioritizeMoves = [&] (const auto& __f, size_t start)
    {
        for (size_t i = start; i < myMoves.size(); i++)
        {
            if (__f(myMoves.pMoves[i]))
                std::swap(myMoves.pMoves[i], myMoves.pMoves[start++]);
        }
        return start;
    };

    const auto SortMoves = [&] (int start, int end)
    {
        for (int i = start + 1; i < end; i++)
        {
            Move key = myMoves.pMoves[i];
            int j = i - 1;

            while ((j >= start) and ((myMoves.pMoves[j] >> 24) < (key >> 24))) {
                myMoves.pMoves[j + 1] = myMoves.pMoves[j];
                --j;
            }
            myMoves.pMoves[j + 1] = key;
        }
    };

    const size_t capture_end = PrioritizeMoves(IsCaptureMove, 0);
    const size_t check_end   = check_moves ? PrioritizeMoves(IsCheckMove, capture_end) : capture_end;
    const size_t pv_end      = pv_moves    ? PrioritizeMoves(IsPvMove, check_end) : check_end;

    SortMoves(0, int(capture_end));
    SortMoves(int(pv_end), int(myMoves.size()));
}


#endif

#ifndef SEARCH_UTIL


bool
OkToDoLMR(Depth depth, MoveList& myMoves)
{
    if (depth < 3) return false;
    // if (myMoves.KingAttackers || myMoves.__count < 6) return false;
    return true;
}

int
RootReduction(Depth depth, size_t num)
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
Reduction (Depth depth, size_t move_no)
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

    answer += 100 * PopCount(pos.piece<WHITE, PAWN  >() | pos.piece<WHITE, PAWN  >());
    answer += 300 * PopCount(pos.piece<WHITE, BISHOP>() | pos.piece<BLACK, BISHOP>());
    answer += 300 * PopCount(pos.piece<WHITE, KNIGHT>() | pos.piece<BLACK, KNIGHT>());
    answer += 500 * PopCount(pos.piece<WHITE, ROOK  >() | pos.piece<BLACK, ROOK  >());
    answer += 900 * PopCount(pos.piece<WHITE, QUEEN >() | pos.piece<BLACK, QUEEN >());
    return answer;
}

#endif

#ifndef SEARCH_COMMON


Score
AlphaBetaNonPV(ChessBoard& _cb, Depth depth, Score alpha, Score beta, int ply)
{
    if (LegalMovesPresent(_cb) == false)
        return _cb.InCheck() ? CheckmateScore(ply) : 0;

    // if (depth <= 0)
    //     return QuieSearch(_cb, alpha, beta, ply, 0);

    auto myMoves = GenerateMoves(_cb);
    OrderMoves(myMoves, false, false);

    for (const Move move : myMoves)
    {
        _cb.MakeMove(move);
        Score eval = -AlphaBetaNonPV(_cb, depth - 1, -beta, -alpha, ply + 1);
        _cb.UnmakeMove();
        
        if (eval > alpha)
            alpha = eval;
        if (eval >= beta)
            return beta;
    }
    
    return alpha;
}

#endif
