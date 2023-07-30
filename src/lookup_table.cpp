

#include "lookup_table.h"

#include <iostream>

namespace plt
{

uint64_t    UpMasks[64];
uint64_t  DownMasks[64];
uint64_t  LeftMasks[64];
uint64_t RightMasks[64];

uint64_t   UpRightMasks[64];
uint64_t    UpLeftMasks[64];
uint64_t DownRightMasks[64];
uint64_t  DownLeftMasks[64];

uint64_t     LineMasks[64];
uint64_t DiagonalMasks[64];

uint64_t   RookMasks[64];
uint64_t BishopMasks[64];
uint64_t KnightMasks[64];
uint64_t   KingMasks[64];

uint64_t        PawnMasks[2][64];
uint64_t PawnCaptureMasks[2][64];

uint64_t   RookStartIndex[64];
uint64_t BishopStartIndex[64];

uint64_t   RookMovesLookUp[106495];
uint64_t BishopMovesLookUp[5248];


uint64_t RookMagics[64] = {
     0x68000814008B4A0, 0x8600114481020020, 0x1F000AC0F1002001, 0x1600020063144099, 0xCE1E254B3BA3A1E2, 0x61000C00A1002812, 0x1E00080200331AA4, 0x5E000102004BA584, 
    0x5DF88002400A8927, 0x1D2700210186C007,  0x27A0025F20081C1, 0x1676002040A8B200, 0xA7AE00044A002150, 0x24AE002912006410, 0x552A000528243200, 0xD7A1002346009500, 
    0x18B189800BE14003, 0x35D5C1C000E01001,   0x79B20042006082, 0x402E420010E02A01, 0x14B501000800910C, 0x9F1B0100189A0400, 0x72AEC40012087330, 0xA0A46A000A91410C, 
    0x553E400080022A82, 0xE7D70602004280A3, 0xECF7F501002000C4, 0x779015DE000DFC41, 0xCEAA002200046930, 0xD949003300083C00, 0xD581D85C00302633, 0x26CE83120005C884, 
    0x6CA9400662800083, 0x15F5874206002300, 0xA81AC02001001502, 0xCC01420012002049, 0x2D55D600460020D2, 0x29B1402438013020, 0xD357769004003817, 0xF0E5CA80F6000401, 
    0x92CE834003E28009, 0xE1DA410586020027, 0x9FE2048040B60020, 0x203FB00104090020, 0xF31CE801005D0010, 0x2CE2007470760028, 0x2E80B1101E640008, 0xBBFC4769285A0014, 
    0xC8B2018700225200, 0x74DE47058200E600, 0x6A3D36008164C200, 0x1A2E5A9BCE003A00, 0x733A003870E02600, 0x6D02001550386200, 0x5058B015B6181C00, 0x3C8088E31F83F600, 
    0x93D60083630254C2, 0x5218204001910381, 0xD13C6832008260C2, 0xE59E00C238522012, 0x701200AC50982006, 0xE6B600348308101E, 0x3E9D78060690091C, 0xFA315A8C0F38A902
};

int RookShifts[64] = {
    52, 53, 53, 52, 52, 53, 53, 52, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    53, 54, 54, 54, 54, 54, 54, 53, 
    52, 53, 53, 53, 53, 53, 53, 52
};

uint64_t BishopMagics[64] = {
    0xC17820058E05C10A, 0xD64E4C58028F0605, 0x2884780208C84BD4, 0x17DC4C0980A3E81E, 0x807C04209FE149EC, 0x9CF4F3E16786090D, 0xED0A5A0620A03652, 0xC124818808051430, 
    0xB51458CE5C081A17, 0xC9A4780AD80366ED, 0x488578184B04A6F3, 0xD36D882081ABFD8B, 0xA3EA63F35E48A68C, 0xD5A8920190A81095, 0x7C731BEBD6EBAF4A, 0x7E7F28340CC8A7C3, 
    0xF816947223301C03, 0xC89456D02C180DDD, 0xF59C13E80A4C0008, 0xC248033C2041F1AA, 0xFD6087D400A012D5, 0x6CC6802348200804, 0xA50C9F012B4EF288, 0x1520457F07480C1E, 
    0x3F200402A8B0AC49, 0x58882415609C23A2, 0xD7B40402C04103A4, 0xFEB8080095820046, 0x3716840001826006, 0xB3E81E0021C1038C, 0xD61A41F1C057F268, 0xCD130345C902D816, 
    0x5276A012DF60D4C8, 0xCC080C32B524481F, 0xD13184010BF006C7, 0x3519010801850040, 0xBB385E0020820080,  0x894080620C60197, 0xEA9C2C1C0E59490F, 0x941AAE07C19A8405, 
    0x9427B8484023D875, 0xBA0A190C6019AE26, 0xD98CB0329002C807, 0x19C93C2017092801, 0xF854820A2C017A00, 0x38B2120351004A03, 0xF850464610C6DC04, 0x21D01AAA014E3880, 
    0xEC6E08031C30547C, 0x8D4E06030B287717, 0xC5DF490A8990423E, 0xCEE5FE45CE719E3B, 0x375AE2D06E020A50, 0xF0A1C6F06A99F114, 0x7FA0890D180389FF, 0x50F03004C0848313, 
    0x165AF40B080A103E, 0x2DD4D902839058AD, 0xFE4FCD7B12231075, 0xB265FF1781840C0B, 0x50E49B7311A6020E, 0x1F1AF5200A161606, 0x7A0F700C07542C00, 0x839CB842080E0010
};

int BishopShifts[64] = {
    58, 59, 59, 59, 59, 59, 59, 58, 
    59, 59, 59, 59, 59, 59, 59, 59, 
    59, 59, 57, 57, 57, 57, 59, 59, 
    59, 59, 57, 55, 55, 57, 59, 59, 
    59, 59, 57, 55, 55, 57, 59, 59, 
    59, 59, 57, 57, 57, 57, 59, 59, 
    59, 59, 59, 59, 59, 59, 59, 59, 
    58, 59, 59, 59, 59, 59, 59, 58
};


static bool
in_range(int __x, int __y)
{ return (__x >= 0) & (__x < 8) & (__y >= 0) & (__y < 8); }


static uint64_t
rook_mask_gen(int sq)
{
	int col = sq & 7, row = (sq - col) >> 3;
	uint64_t res = 0;

    for (int i = row + 1, j = col; i < 7; i++)
        res |= 1ULL << (8 * i + j);
    
    for (int i = row - 1, j = col; i > 0; i--)
        res |= 1ULL << (8 * i + j);
    
    for (int i = row, j = col + 1; j < 7; j++)
        res |= 1ULL << (8 * i + j);
    
    for (int i = row, j = col - 1; j > 0; j--)
        res |= 1ULL << (8 * i + j);

    return res;
}

static uint64_t
bishop_mask_gen(int sq)
{
	int col = sq & 7, row = (sq - col) >> 3;
    uint64_t res = 0;

    for (int i = row + 1, j = col + 1; i < 7 and j < 7; i++, j++)
        res |= 1ULL << (8 * i + j);

    for (int i = row + 1, j = col - 1; i < 7 and j > 0; i++, j--)
        res |= 1ULL << (8 * i + j);
    
    for (int i = row - 1, j = col + 1; i > 0 and j < 7; i--, j++)
        res |= 1ULL << (8 * i + j);
    
    for (int i = row - 1, j = col - 1; i > 0 and j > 0; i--, j--)
        res |= 1ULL << (8 * i + j);

    return res;
}

static int
generate_blockers(uint64_t mask, uint64_t* blockers)
{
    // Any sq can have a max of 12 sq available for a rook piece
    uint64_t list[12]{};
    int __n = 0;

    while (mask != 0)
    {
        list[__n++] = mask ^ (mask & (mask - 1));
        mask &= mask - 1;
    }

    int __cn = 0;

    for (int i = 0; i < (1 << __n); i++)
    {
    	uint64_t res = 0;

    	for (int j = 0; j < __n; j++)
    		if ((1 << j) & i) res |= list[j];

    	blockers[__cn++] = res;
    }

    return __cn;
}

static void
build_rook_bishop_masks()
{
    for (int sq = 0; sq < 64; sq++) {
        RookMasks[sq] = rook_mask_gen(sq);
        BishopMasks[sq] = bishop_mask_gen(sq);
    }
}

static uint64_t
loop_result(int sq, int ix, int iy, uint64_t blocker)
{
	uint64_t res = 0;
	int col = (sq & 7), row = (sq - col) >> 3;

	for (int i = row + ix, j = col + iy; in_range(i, j); i += ix, j += iy)
	{
		uint64_t __x = 1ULL << (8 * i + j);
		res |= 1ULL << (8 * i + j);
		if (__x & blocker) break;
	}

	return res;
}

static uint64_t
rook_squares_basic(int sq, uint64_t blocker)
{
	return loop_result(sq, 1, 0, blocker) | loop_result(sq, -1, 0, blocker)
		 | loop_result(sq, 0, 1, blocker) | loop_result(sq, 0, -1, blocker);
}

static uint64_t
bishop_squares_basic(int sq, uint64_t blocker)
{
	return loop_result(sq, 1, 1, blocker) | loop_result(sq, -1, -1, blocker)
		 | loop_result(sq, 1, -1, blocker) | loop_result(sq, -1, 1, blocker);
}

static void
build_lookup_table(uint64_t* masks, uint64_t* magic_table, int* shift_table, uint64_t* start_index,
                   uint64_t* lookup_table, uint64_t (*atk_square_gen)(int, uint64_t))
{
	uint64_t blockers[4096];

    start_index[0] = 0;

    for (int sq = 0; sq < 64; ++sq)
	{
		int __n = generate_blockers(masks[sq], blockers);

		int bits = shift_table[sq];
		uint64_t magic = magic_table[sq];
		uint64_t m_index = 0;

		for (int i = 0; i < __n; i++)
		{
			uint64_t occupancy = start_index[sq] + ((blockers[i] * magic) >> bits);
			m_index = std::max(m_index, occupancy);

            uint64_t result = atk_square_gen(sq, blockers[i]);
			lookup_table[occupancy] = result;
		}

        if (sq < 63)
            start_index[sq + 1] = m_index + 1;
	}
}


static void
build_sliding_table(uint64_t* _arr, int index, int index_inc, int inc_x, int inc_y)
{
    for (int idx = index;; idx += index_inc)
    {
        if (idx < 0 || idx >= 64) break;
        int x = (idx & 7), y = (idx - x) >> 3;
        uint64_t val = 0;
        for (int i = x, j = y; in_range(i, j); i += inc_x, j += inc_y)
        {
            _arr[8 * j + i] = val;
            val |= 1ULL << (8 * j + i);
        }
    }
}


static void
build_knight_table(uint64_t* _arr)
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
build_king_table(uint64_t* _arr)
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
build_pawn_table(uint64_t* _arr, int dir, bool captures)
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
merge_table(uint64_t* to_table, uint64_t* from_table1, uint64_t* from_table2)
{
    for (int i = 0; i < 64; i++)
        to_table[i] |= from_table1[i] | from_table2[i];
}


static void
set_zero(uint64_t* table)
{
    for (int i = 0; i < 64; i++)
        table[i] = 0;
}


void
init()
{    
    build_sliding_table(   UpMasks, 56,  1,  0, -1);
    build_sliding_table( DownMasks,  7, -1,  0,  1);
    build_sliding_table(RightMasks,  7,  8, -1,  0);
    build_sliding_table( LeftMasks,  0,  8,  1,  0);

    build_sliding_table(  UpRightMasks, 63, -8, -1, -1);
    build_sliding_table(  UpRightMasks, 56,  1, -1, -1);
    build_sliding_table( DownLeftMasks,  0,  8,  1,  1);
    build_sliding_table( DownLeftMasks,  7, -1,  1,  1);
    build_sliding_table(   UpLeftMasks, 56,  1,  1, -1);
    build_sliding_table(   UpLeftMasks, 56, -8,  1, -1);
    build_sliding_table(DownRightMasks,  7,  8, -1,  1);
    build_sliding_table(DownRightMasks,  7, -1, -1,  1);

    build_knight_table(KnightMasks);
    build_king_table(KingMasks);

    build_pawn_table( PawnMasks[1],  1, false);
    build_pawn_table( PawnMasks[0], -1, false);
    build_pawn_table(PawnCaptureMasks[1],  1,  true);
    build_pawn_table(PawnCaptureMasks[0], -1,  true);

    set_zero(DiagonalMasks);
    set_zero(LineMasks);

    merge_table(DiagonalMasks,   UpRightMasks,   UpLeftMasks);
    merge_table(DiagonalMasks, DownRightMasks, DownLeftMasks);

    merge_table(LineMasks,   UpMasks,  DownMasks);
    merge_table(LineMasks, LeftMasks, RightMasks);

    build_rook_bishop_masks();
    build_lookup_table(  RookMasks,   RookMagics,   RookShifts,   RookStartIndex,   RookMovesLookUp,   rook_squares_basic);
    build_lookup_table(BishopMasks, BishopMagics, BishopShifts, BishopStartIndex, BishopMovesLookUp, bishop_squares_basic);
}

}

