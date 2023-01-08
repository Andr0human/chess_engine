

#include "lookup_table.h"

uint64_t plt::NtBoard[64], plt::KBoard[64];
uint64_t plt::uBoard[64], plt::dBoard[64], plt::rBoard[64], plt::lBoard[64];
uint64_t plt::urBoard[64], plt::drBoard[64], plt::ulBoard[64], plt::dlBoard[64];
uint64_t plt::diag_Board[64], plt::line_Board[64];
uint64_t plt::pBoard[2][64], plt::pcBoard[2][64];
uint64_t plt::d_ry[64], plt::ad_ry[64];
uint64_t plt::LRboard[8][258], plt::UDboard[8][258];
// uint64_t plt::ad_bd[8][258], plt::d_bd[8][258];

void
plt::build_sliding_table(uint64_t _arr[], int index, int index_inc, int inc_x, int inc_y)
{
    for (int idx = index;; idx += index_inc)
    {
        if (idx < 0 || idx >= 64) break;
        int x = (idx & 7), y = (idx - x) >> 3;
        uint64_t val = 0;
        for (int i = x, j = y;; i += inc_x, j += inc_y)
        {
            if (i < 0 || j < 0 || i >= 8 || j >= 8) break;
            _arr[8 * j + i] = val;
            val |= 1ULL << (8 * j + i);
        }
    }
}

void
plt::build_knight_king_table(uint64_t _arr[], int nt)
{
    // nt = 1, builds table for knight, nt = 0, build table for king

    for (int i = 0; i < 64; i++)
    {
        int x = i & 7, y = (i - x) >> 3;
        uint64_t val = 0;
        if (x + 1 + nt < 8 && y + 1 < 8) val |= 1ULL << (x + 1 + nt + 8 * (y + 1));
        if (x + 1 + nt < 8 && y - nt >= 0) val |= 1ULL << (x + 1 + nt + 8 * (y - nt));
        if (x - 1 - nt >= 0 && y + nt < 8) val |= 1ULL << (x - 1 - nt + 8 * (y + nt));
        if (x - 1 - nt >= 0 && y - 1 >= 0) val |= 1ULL << (x - 1 - nt + 8 * (y - 1));
        if (x + nt < 8 && y + 1 + nt < 8) val |= 1ULL << (x + nt + 8 * (y + 1 + nt));
        if (x + 1 < 8 && y - 1 - nt >= 0) val |= 1ULL << (x + 1 + 8 * (y - 1 - nt));
        if (x - 1 >= 0 && y + 1 + nt < 8) val |= 1ULL << (x - 1 + 8 * (y + 1 + nt));
        if (x - nt >= 0 && y - 1 - nt >= 0) val |= 1ULL << (x - nt + 8 * (y - 1 - nt));
        _arr[i] = val;
    }
}

void
plt::build_pawn_table(uint64_t _arr[], int dir, bool captures)
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

void
plt::merge_table(uint64_t to_table[], uint64_t from_table1[], uint64_t from_table2[])
{
    for (int i = 0; i < 64; i++)
        to_table[i] |= from_table1[i] | from_table2[i];
}

void
plt::set_zero(uint64_t table[])
{
    for (int i = 0; i < 64; i++)
        table[i] = 0;
}

uint64_t
plt::solution(int idx, uint64_t Apieces)
{
    uint64_t res, val, ans = plt::rBoard[idx] ^ plt::lBoard[idx];
    
    res = plt::rBoard[idx] & Apieces;
    if (res)
    {
        val = res ^ (res & (res - 1));
        ans ^= plt::rBoard[__builtin_popcountll(val - 1)];
    }
    res = plt::lBoard[idx] & Apieces;
    if (res)
    {
        val = (res ? (1ULL << (63 - __builtin_clzll(res))) : 0);
        ans ^= plt::lBoard[__builtin_popcountll(val - 1)];
    }
    
    return ans;
}

uint64_t
plt::convert_to(uint64_t N, int a, int b)
{
    uint64_t res = 0;

    while (N)
    {
        uint64_t val = N ^ (N & (N - 1));
        res |= 1ULL << (a * (__builtin_popcountll(val - 1) + b));
        N &= N - 1;
    }
    return res;
}

void
plt::generate_sol_array(uint64_t _arr[8][256])
{
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 256; j++)
            _arr[i][j] = solution(i, j);
}

void
plt::build_rook_bishop_table(uint64_t sol_arr[8][256], uint64_t _arr[8][258], uint64_t mod, int a, int b)
{
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 256; j++)
        {
            uint64_t sol = sol_arr[i][j];
            uint64_t converted = mod ? convert_to(sol, a, b) : sol;
            uint64_t index = mod ? convert_to(j, a, b) % mod : j;
            _arr[i][index] = converted;
        }
    }
}

void
plt::init()
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

    build_knight_king_table(NtBoard, 1);
    build_knight_king_table( KBoard, 0);

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

    // uint64_t sol_array[8][256];
    // generate_sol_array(sol_array);
    // build_rook_bishop_table(sol_array, plt::LRboard, 0, 1, 0);
    // build_rook_bishop_table(sol_array, plt::UDboard, 258, 8, 0);
    // build_rook_bishop_table(sol_array, plt::d_bd, 257, 7, 1);
    // build_rook_bishop_table(sol_array, plt::ad_bd, 258, 9, 0);
}

