

#include "search_utils.h"


SearchData info;
MoveOrderClass moc;


#ifndef MOVE_ORDER_CLASS

void
MoveOrderClass::Initialise(MoveList& myMoves)
{
    mCount = myMoves.size();
    for (uint64_t i = 0; i < mCount; i++)
        moves[i] = std::make_pair(filter(myMoves.pMoves[i]), 0);
}

void
MoveOrderClass::Insert(uint64_t idx, double time_taken)
{ moves[idx].second = time_taken; }

void
MoveOrderClass::SortList(int best_move)
{
    for (uint64_t i = 0; i < mCount; i++)
    {
        if (filter(best_move) == filter(moves[i].first))
        {
            std::swap(moves[i], moves[0]);
            break;
        }
    }

    for (uint64_t i = 1; i < mCount; i++)
    {
        uint64_t idx = i;
        for (uint64_t j = i; j < mCount; j++)
            if (moves[j].second > moves[idx].second) idx = j;
        std::swap(moves[i], moves[idx]);
    }
}

void
MoveOrderClass::OrderMovesOnTime(MoveList& myMoves)
{
    for (uint64_t i = 0; i < mCount; i++) {
        Move current = moves[i].first;
        for (uint64_t j = i + 1; j < mCount; j++)
            if (myMoves.pMoves[j] == current)
                std::swap(myMoves.pMoves[j], myMoves.pMoves[i]);
    }
}

void
MoveOrderClass::PrintAllMoves(ChessBoard& cb)
{
    for (uint64_t i = 0; i < mCount; i++)
        cout << i + 1 << ") " << PrintMove(moves[i].first, cb)
             << " | Time : " << moves[i].second << endl;
}

int
MoveOrderClass::GetMove(uint64_t index)
{ return moves[index].first; }

uint64_t
MoveOrderClass::MoveCount()
{ return mCount; }

void
MoveOrderClass::Reset()
{
    for (uint64_t i = 0; i < mCount; i++)
        moves[i].second = 0;
}

#endif

#ifndef TEST_POSITION

TestPosition::TestPosition(string f, int d, uint64_t nc, int index)
{
    fen = f;
    depth = d;
    nodeCount = nc;
    GenerateName(index);
}

TestPosition::TestPosition(string f, int d, uint64_t nc, string n)
{
    fen = f;
    name = n;
    depth = d;
    nodeCount = nc;
}

void
TestPosition::GenerateName(int index)
{
    if (index == 1) name = "pawn";
    else if (index == 2) name = "bishop";
    else if (index == 3) name = "knight";
    else if (index == 4) name = "rook";
    else if (index == 5) name = "queen";
    else name = "--";
}

void
TestPosition::Print()
{
    cout << name << " : " << fen << "\n" << "|   Depth : " \
    << depth << "\t|   Nodes : " << nodeCount << "\t|\n" << endl;
}

#endif

