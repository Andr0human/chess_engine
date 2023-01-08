

#include "lookup_table.h"

#include <iostream>

namespace plt
{

uint64_t NtBoard[64], KBoard[64];
uint64_t uBoard[64], dBoard[64], rBoard[64], lBoard[64];
uint64_t urBoard[64], drBoard[64], ulBoard[64], dlBoard[64];
uint64_t diag_Board[64], line_Board[64];
uint64_t pBoard[2][64], pcBoard[2][64];
uint64_t d_ry[64], ad_ry[64];
// uint64_t LRboard[8][258], UDboard[8][258];
// uint64_t ad_bd[8][258], d_bd[8][258];

static bool
in_range(int __x, int __y)
{ return (__x >= 0) & (__x < 8) & (__y >= 0) & (__y < 8); }


static void
build_sliding_table(uint64_t _arr[], int index, int index_inc, int inc_x, int inc_y)
{
    for (int idx = index;; idx += index_inc)
    {
        if (idx < 0 || idx >= 64) break;
        int x = (idx & 7), y = (idx - x) >> 3;
        uint64_t val = 0;
        for (int i = x, j = y;; i += inc_x, j += inc_y)
        {
            if (in_range(i, j) == false)
                break;
            _arr[8 * j + i] = val;
            val |= 1ULL << (8 * j + i);
        }
    }
}


static void
build_knight_table(uint64_t _arr[])
{
    const int inc_x[2] = {2, -2};
    const int inc_y[2] = {1, -1};

    for (int square = 0; square < 64; square++)
    {
        int col = square & 7;
        int row = (square - col) >> 3;
        uint64_t value = 0;

        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                if (in_range(row + inc_x[i], col + inc_y[j]))
                    value |= 1ULL << (8 * (row + inc_x[i]) + (col + inc_y[j]));

                if (in_range(row + inc_y[j], col + inc_x[i]))
                    value |= 1ULL << (8 * (row + inc_y[j]) + (col + inc_x[i]));
            }
        }

        _arr[square] = value;
    }
}


static void
build_king_table(uint64_t _arr[])
{
    const int inc[2] = {1, -1};

    for (int square = 0; square < 64; square++)
    {
        int col = square & 7;
        int row = (square - col) >> 3;
        uint64_t value = 0;

        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                if (in_range(row + inc[i], col + inc[j]))
                    value |= 1ULL << (8 * (row + inc[i]) + (col + inc[j]));
            }

            if (in_range(row + inc[i], col))
                value |= 1ULL << (8 * (row + inc[i]) + (col));

            if (in_range(row, col + inc[i]))
                value |= 1ULL << (8 * (row) + (col + inc[i]));
        }

        _arr[square] = value;
    }
}


static void
build_pawn_table(uint64_t _arr[], int dir, bool captures)
{    
    for (int i = 0; i < 64; i++)
    {
        int x = i & 7, y = (i - x) >> 3, up = i + 8 * dir;
        uint64_t val = 0;
        if (captures)
        {
            if (x - 1 >= 0) val |= 1ULL << (x - 1 + 8 * (y + dir));
            if (x + 1 < 8)  val |= 1ULL << (x + 1 + 8 * (y + dir));
        }
        else if (0 <= up && up < 64)
        {
            val |= 1ULL << up;
        }
        _arr[i] = val;
    }
}


static void
merge_table(uint64_t to_table[], uint64_t from_table1[], uint64_t from_table2[])
{
    for (int i = 0; i < 64; i++)
        to_table[i] |= from_table1[i] | from_table2[i];
}


static void
set_zero(uint64_t table[])
{
    for (int i = 0; i < 64; i++)
        table[i] = 0;
}


void
init()
{    
    build_sliding_table(uBoard, 56,  1,  0, -1);
    build_sliding_table(dBoard,  7, -1,  0,  1);
    build_sliding_table(rBoard,  7,  8, -1,  0);
    build_sliding_table(lBoard,  0,  8,  1,  0);

    build_sliding_table(urBoard, 63, -8, -1, -1);
    build_sliding_table(urBoard, 56,  1, -1, -1);
    build_sliding_table(dlBoard,  0,  8,  1,  1);
    build_sliding_table(dlBoard,  7, -1,  1,  1);
    build_sliding_table(ulBoard, 56,  1,  1, -1);
    build_sliding_table(ulBoard, 56, -8,  1, -1);
    build_sliding_table(drBoard,  7,  8, -1,  1);
    build_sliding_table(drBoard,  7, -1, -1,  1);


    build_knight_table(NtBoard);
    build_king_table(KBoard);

    build_pawn_table( pBoard[1],  1, false);
    build_pawn_table( pBoard[0], -1, false);
    build_pawn_table(pcBoard[1],  1,  true);
    build_pawn_table(pcBoard[0], -1,  true);

    set_zero(d_ry);
    set_zero(ad_ry);
    set_zero(diag_Board);
    set_zero(line_Board);

    merge_table( d_ry, ulBoard, drBoard);
    merge_table(ad_ry, urBoard, dlBoard);

    merge_table(diag_Board, urBoard, ulBoard);
    merge_table(diag_Board, drBoard, dlBoard);

    merge_table(line_Board, uBoard, dBoard);
    merge_table(line_Board, lBoard, rBoard);
}


}

