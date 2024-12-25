
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
CheckmateScore(Ply ply)
{ return -VALUE_MATE + (20 * ply); }

#endif

#ifndef SEARCH_UTIL

template <int flag>
static bool
is_type(Move m)
{ return (m & (flag << 21)) != 0; }

template <>
bool
is_type<PV_MOVE>(Move m)
{ return info.IsPartOfPV(m); }


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
Reduction (Depth depth, size_t moveNo)
{
    // LMR ok after depth 2 and LMR_LIMIT: 4
    if (depth <= 4) 
        return moveNo < 12 ? 1 : 2;

    if (depth <= 8) {
        if (moveNo < 8) return 1;
        if (moveNo < 12) return 2;
        if (moveNo < 18) return 3;
        return 4;
    }

    if (depth <= 12) {
        if (moveNo < 8) return 2;
        if (moveNo < 24) return 3;
        return 4;
    }

    if (moveNo < 8) return 2;
    return 4;
}


bool
LmrOk(Move move, Depth depth, size_t move_no)
{
    if ((depth <= 2) or (move_no < LMR_LIMIT) or InterestingMove(move))
        return false;

    return true;
}

bool
InterestingMove(Move move)
{
    if (is_type<CAPTURES>(move) or is_type<CHECK>(move))
        return true;

    if (is_type<CASTLING>(move) or is_type<PROMOTION>(move))
        return true;
    
    return false;
}


int
SearchExtension(const MoveList& myMoves, int numExtensions)
{
    int extension = 0;

    if (numExtensions >= EXTENSION_LIMIT)
        return 0;
    
    // If king is in check, add 1
    if (myMoves.checkers > 0)
        extension += 1;
    
    return extension;
}

#endif

#ifndef MOVE_REORDERING


template <int flag>
static size_t
PrioritizeMoves(MoveList& myMoves, size_t start)
{
    for (size_t i = start; i < myMoves.size(); i++)
    {
        if (is_type<flag>(myMoves.pMoves[i]))
            std::swap(myMoves.pMoves[i], myMoves.pMoves[start++]);
    }
    return start;
}


void
OrderMoves(MoveList& myMoves, bool pv_moves, bool check_moves)
{
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

    const size_t capture_end = PrioritizeMoves<CAPTURES>(myMoves, 0);
    const size_t check_end   = check_moves ? PrioritizeMoves<CHECK>(myMoves, capture_end) : capture_end;
    const size_t pv_end      = pv_moves    ? PrioritizeMoves<PV_MOVE>(myMoves, check_end) : check_end;

    SortMoves(0, int(capture_end));
    SortMoves(int(pv_end), int(myMoves.size()));
}


#endif
