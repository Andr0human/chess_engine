
#include "search.h"

// template <>
// bool
// is_type<PV_MOVE>(Move m)
// { return info.IsPartOfPV(m); }

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
