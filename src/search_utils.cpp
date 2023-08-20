

#include "search_utils.h"


SearchData info;
MoveOrderClass moc;


#ifndef MOVE_ORDER_CLASS

void
MoveOrderClass::initialise(MoveList& myMoves)
{
    mCount = myMoves.size();
    for (uint64_t i = 0; i < mCount; i++)
        moves[i] = std::make_pair(myMoves.pMoves[i], 0);
}

void
MoveOrderClass::insert(uint64_t idx, double time_taken)
{ moves[idx].second = time_taken; }

void
MoveOrderClass::sortList(int best_move)
{
    for (uint64_t i = 0; i < mCount; i++)
    {
        if (best_move == moves[i].first)
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
MoveOrderClass::setMoveOrder(MoveList& myMoves)
{
    for (uint64_t i = 0; i < mCount; i++) {
        int curr = moves[i].first;
        for (uint64_t j = i + 1; j < mCount; j++)
            if (myMoves.pMoves[j] == curr)
                std::swap(myMoves.pMoves[j], myMoves.pMoves[i]);
    }
}

void
MoveOrderClass::mprint(ChessBoard& cb)
{
    for (uint64_t i = 0; i < mCount; i++)
        cout << i + 1 << ") " << print(moves[i].first, cb)
             << " | Time : " << moves[i].second << endl;
}

int
MoveOrderClass::get_move(uint64_t index)
{ return moves[index].first; }

uint64_t
MoveOrderClass::moveCount()
{ return mCount; }

void
MoveOrderClass::reset()
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
    generate_name(index);
}

TestPosition::TestPosition(string f, int d, uint64_t nc, string n)
{
    fen = f;
    name = n;
    depth = d;
    nodeCount = nc;
}

void
TestPosition::generate_name(int index)
{
    if (index == 1) name = "pawn";
    else if (index == 2) name = "bishop";
    else if (index == 3) name = "knight";
    else if (index == 4) name = "rook";
    else if (index == 5) name = "queen";
    else name = "--";
}

void
TestPosition::print()
{
    cout << name << " : " << fen << "\n" << "|   Depth : " \
    << depth << "\t|   Nodes : " << nodeCount << "\t|\n" << endl;
}

#endif



int
ExtensionDepth(const ChessBoard& pos, int numExtension)
{
    if (numExtension >= EXTENSION_LIMIT)
        return 0;

    if (pos.king_in_check())
        return 1;
    
    return 0;
}

