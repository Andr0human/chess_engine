
#include "lookup_table.h"

namespace plt
{

MaskTable    upMasks;
MaskTable  downMasks;
MaskTable  leftMasks;
MaskTable rightMasks;

MaskTable   upRightMasks;
MaskTable    upLeftMasks;
MaskTable downRightMasks;
MaskTable  downLeftMasks;

MaskTable     lineMasks;
MaskTable diagonalMasks;

MaskTable      rookMasks;
MaskTable    bishopMasks;
MaskTable    knightMasks;
MaskTable      kingMasks;
MaskTable kingOuterMasks;

array<MaskTable, COLOR_NB>        pawnMasks;
array<MaskTable, COLOR_NB> pawnCaptureMasks;
array<MaskTable, COLOR_NB>  passedPawnMasks;

MaskTable   rookStartIndex;
MaskTable bishopStartIndex;

Bitboard*   rookMovesLookUp = new Bitboard[ROOK_LOOKUP_TABLE_SIZE  ];
Bitboard* bishopMovesLookUp = new Bitboard[BISHOP_LOOKUP_TABLE_SIZE];


MaskTable rookMagics = {
   0x68000814008B4A0, 0x8600114481020020, 0x1F000AC0F1002001, 0x1600020063144099, 0xCE1E254B3BA3A1E2, 0x61000C00A1002812, 0x1E00080200331AA4, 0x5E000102004BA584,
  0x5DF88002400A8927, 0x1D2700210186C007,  0x27A0025F20081C1, 0x1676002040A8B200, 0xA7AE00044A002150, 0x24AE002912006410, 0x552A000528243200, 0xD7A1002346009500,
  0x18B189800BE14003, 0x35D5C1C000E01001,   0x79B20042006082, 0x402E420010E02A01, 0x14B501000800910C, 0x9F1B0100189A0400, 0x72AEC40012087330, 0xA0A46A000A91410C,
  0x553E400080022A82, 0xE7D70602004280A3, 0xECF7F501002000C4, 0x779015DE000DFC41, 0xCEAA002200046930, 0xD949003300083C00, 0xD581D85C00302633, 0x26CE83120005C884,
  0x6CA9400662800083, 0x15F5874206002300, 0xA81AC02001001502, 0xCC01420012002049, 0x2D55D600460020D2, 0x29B1402438013020, 0xD357769004003817, 0xF0E5CA80F6000401,
  0x92CE834003E28009, 0xE1DA410586020027, 0x9FE2048040B60020, 0x203FB00104090020, 0xF31CE801005D0010, 0x2CE2007470760028, 0x2E80B1101E640008, 0xBBFC4769285A0014,
  0xC8B2018700225200, 0x74DE47058200E600, 0x6A3D36008164C200, 0x1A2E5A9BCE003A00, 0x733A003870E02600, 0x6D02001550386200, 0x5058B015B6181C00, 0x3C8088E31F83F600,
  0x93D60083630254C2, 0x5218204001910381, 0xD13C6832008260C2, 0xE59E00C238522012, 0x701200AC50982006, 0xE6B600348308101E, 0x3E9D78060690091C, 0xFA315A8C0F38A902
};

ShiftTable rookShifts = {
  52, 53, 53, 52, 52, 53, 53, 52,
  53, 54, 54, 54, 54, 54, 54, 53,
  53, 54, 54, 54, 54, 54, 54, 53,
  53, 54, 54, 54, 54, 54, 54, 53,
  53, 54, 54, 54, 54, 54, 54, 53,
  53, 54, 54, 54, 54, 54, 54, 53,
  53, 54, 54, 54, 54, 54, 54, 53,
  52, 53, 53, 53, 53, 53, 53, 52
};

MaskTable bishopMagics = {
  0xC17820058E05C10A, 0xD64E4C58028F0605, 0x2884780208C84BD4, 0x17DC4C0980A3E81E, 0x807C04209FE149EC, 0x9CF4F3E16786090D, 0xED0A5A0620A03652, 0xC124818808051430,
  0xB51458CE5C081A17, 0xC9A4780AD80366ED, 0x488578184B04A6F3, 0xD36D882081ABFD8B, 0xA3EA63F35E48A68C, 0xD5A8920190A81095, 0x7C731BEBD6EBAF4A, 0x7E7F28340CC8A7C3,
  0xF816947223301C03, 0xC89456D02C180DDD, 0xF59C13E80A4C0008, 0xC248033C2041F1AA, 0xFD6087D400A012D5, 0x6CC6802348200804, 0xA50C9F012B4EF288, 0x1520457F07480C1E,
  0x3F200402A8B0AC49, 0x58882415609C23A2, 0xD7B40402C04103A4, 0xFEB8080095820046, 0x3716840001826006, 0xB3E81E0021C1038C, 0xD61A41F1C057F268, 0xCD130345C902D816,
  0x5276A012DF60D4C8, 0xCC080C32B524481F, 0xD13184010BF006C7, 0x3519010801850040, 0xBB385E0020820080,  0x894080620C60197, 0xEA9C2C1C0E59490F, 0x941AAE07C19A8405,
  0x9427B8484023D875, 0xBA0A190C6019AE26, 0xD98CB0329002C807, 0x19C93C2017092801, 0xF854820A2C017A00, 0x38B2120351004A03, 0xF850464610C6DC04, 0x21D01AAA014E3880,
  0xEC6E08031C30547C, 0x8D4E06030B287717, 0xC5DF490A8990423E, 0xCEE5FE45CE719E3B, 0x375AE2D06E020A50, 0xF0A1C6F06A99F114, 0x7FA0890D180389FF, 0x50F03004C0848313,
  0x165AF40B080A103E, 0x2DD4D902839058AD, 0xFE4FCD7B12231075, 0xB265FF1781840C0B, 0x50E49B7311A6020E, 0x1F1AF5200A161606, 0x7A0F700C07542C00, 0x839CB842080E0010
};

ShiftTable bishopShifts = {
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
inRange(int __x, int __y)
{ return (__x >= 0) & (__x < 8) & (__y >= 0) & (__y < 8); }


static Bitboard
rookMaskGen(Square sq)
{
	int col = sq & 7, row = (sq - col) >> 3;
	Bitboard res = 0;

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

static Bitboard
bishopMaskGen(Square sq)
{
	int col = sq & 7, row = (sq - col) >> 3;
  Bitboard res = 0;

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
generateBlockers(Bitboard mask, Bitboard* blockers)
{
  // Any sq can have a max of 12 sq available for a rook piece
  Bitboard list[12]{};
  int n = 0;

  while (mask != 0)
  {
    list[n++] = mask ^ (mask & (mask - 1));
    mask &= mask - 1;
  }

  int cn = 0;

  for (int i = 0; i < (1 << n); i++)
  {
    Bitboard res = 0;

    for (int j = 0; j < n; j++)
      if ((1 << j) & i) res |= list[j];

    blockers[cn++] = res;
  }

  return cn;
}

static void
buildRookBishopMasks()
{
  for (Square sq = SQUARE_ZERO; sq < SQUARE_NB; ++sq) {
    rookMasks[sq]   = rookMaskGen(sq);
    bishopMasks[sq] = bishopMaskGen(sq);
  }
}

static Bitboard
loopResult(Square sq, int ix, int iy, Bitboard blocker)
{
	Bitboard res = 0;
	int col = (sq & 7), row = (sq - col) >> 3;

	for (int i = row + ix, j = col + iy; inRange(i, j); i += ix, j += iy)
	{
		Bitboard x = 1ULL << (8 * i + j);
		res |= 1ULL << (8 * i + j);
		if (x & blocker) break;
	}

	return res;
}

static Bitboard
rookSquaresBasic(Square sq, Bitboard blocker)
{
	return loopResult(sq, 1, 0, blocker) | loopResult(sq, -1, 0, blocker)
		   | loopResult(sq, 0, 1, blocker) | loopResult(sq, 0, -1, blocker);
}

static Bitboard
bishopSquaresBasic(Square sq, Bitboard blocker)
{
	return loopResult(sq, 1, 1, blocker) | loopResult(sq, -1, -1, blocker)
		   | loopResult(sq, 1, -1, blocker) | loopResult(sq, -1, 1, blocker);
}

static void
buildLookUpTable(MaskTable& masks, MaskTable& magicTable, ShiftTable& shiftTable, MaskTable& startIndex,
                 Bitboard* lookupTable, Bitboard (*atkSquareGen)(Square, Bitboard))
{
	Bitboard blockers[4096];
  startIndex[0] = 0;

  for (Square sq = SQ_A1; sq < SQUARE_NB; ++sq)
	{
		int n = generateBlockers(masks[sq], blockers);

		int bits = shiftTable[sq];
		Bitboard magic = magicTable[sq];
		uint64_t mIndex = 0;

		for (int i = 0; i < n; i++)
		{
			Bitboard occupancy = startIndex[sq] + ((blockers[i] * magic) >> bits);

      if (occupancy > mIndex)
        mIndex = occupancy;

      Bitboard result = atkSquareGen(sq, blockers[i]);
			lookupTable[occupancy] = result;
		}

    if (sq < 63)
      startIndex[sq + 1] = mIndex + 1;
	}
}


static void
buildSlidingTable(MaskTable& arr, int index, int indexInc, int incX, int incY)
{
  for (int idx = index;; idx += indexInc)
  {
    if (idx < 0 || idx >= SQUARE_NB) break;
    int x = (idx & 7), y = (idx - x) >> 3;
    Bitboard val = 0;
    for (int i = x, j = y; inRange(i, j); i += incX, j += incY)
    {
      arr[8 * j + i] = val;
      val |= 1ULL << (8 * j + i);
    }
  }
}


static void
buildKnightTable(MaskTable& arr)
{
  const int inc_x[2] = {2, -2};
  const int inc_y[2] = {1, -1};

  for (Square square = SQ_A1; square < SQUARE_NB; ++square)
  {
    int col = square & 7;
    int row = (square - col) >> 3;
    Bitboard value = 0;

    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < 2; j++)
      {
        if (inRange(row + inc_x[i], col + inc_y[j]))
          value |= 1ULL << (8 * (row + inc_x[i]) + (col + inc_y[j]));

        if (inRange(row + inc_y[j], col + inc_x[i]))
          value |= 1ULL << (8 * (row + inc_y[j]) + (col + inc_x[i]));
      }
    }

    arr[square] = value;
  }
}


static void
buildKingTable(MaskTable& arr)
{
  const int inc[SQUARE_NB] = {1, -1};

  for (Square square = SQ_A1; square < SQUARE_NB; ++square)
  {
    int col = square & 7;
    int row = (square - col) >> 3;
    Bitboard value = 0;

    for (int i = 0; i < 2; i++)
    {
      for (int j = 0; j < 2; j++)
      {
        if (inRange(row + inc[i], col + inc[j]))
          value |= 1ULL << (8 * (row + inc[i]) + (col + inc[j]));
      }

      if (inRange(row + inc[i], col))
        value |= 1ULL << (8 * (row + inc[i]) + (col));

      if (inRange(row, col + inc[i]))
        value |= 1ULL << (8 * (row) + (col + inc[i]));
    }

    arr[square] = value;
  }
}


static void
buildKingOuterTable(MaskTable& table)
{
  for (Square square = SQ_A1; square < SQUARE_NB; ++square)
  {
    int col = square & 7;
    int row = (square - col) >> 3;

    Bitboard value = VALUE_ZERO;

    if (inRange(row + 1, col + 1)) value |= kingMasks[square + 9];
    if (inRange(row + 1, col - 1)) value |= kingMasks[square + 7];
    if (inRange(row - 1, col + 1)) value |= kingMasks[square - 7];
    if (inRange(row - 1, col - 1)) value |= kingMasks[square - 9];

    table[square] = value ^ (value & (kingMasks[square] | (1ULL << square)));
  }
}


static void
buildPawnTable(MaskTable& arr, int dir, bool captures)
{
  for (int i = 0; i < SQUARE_NB; i++)
  {
    int x = i & 7, y = (i - x) >> 3, up = i + 8 * dir;
    Bitboard val = 0;
    if (captures)
    {
      if (x - 1 >= 0) val |= 1ULL << (x - 1 + 8 * (y + dir));
      if (x + 1 < 8)  val |= 1ULL << (x + 1 + 8 * (y + dir));
    }
    else if (0 <= up && up < SQUARE_NB)
    {
      val |= 1ULL << up;
    }
    arr[i] = val;
  }
}


static void
buildPassedPawnTable(MaskTable& arr, MaskTable& genTable)
{
  for (Square sq = SQ_A1; sq < SQUARE_NB; ++sq)
  {
    Bitboard val = genTable[sq];

    if ((sq & 7) < 7) val |= genTable[sq + 1];
    if ((sq & 7) > 0) val |= genTable[sq - 1];
    arr[sq] = val;
  }
}


static void
mergeTable(MaskTable& toTable, MaskTable& fromTable1, MaskTable& fromTable2)
{
  for (int i = 0; i < SQUARE_NB; i++)
    toTable[i] |= fromTable1[i] | fromTable2[i];
}


static void
setZero(MaskTable& table)
{
  for (int i = 0; i < SQUARE_NB; i++)
    table[i] = 0;
}


void
init()
{
  buildSlidingTable(   upMasks, 56,  1,  0, -1);
  buildSlidingTable( downMasks,  7, -1,  0,  1);
  buildSlidingTable(rightMasks,  7,  8, -1,  0);
  buildSlidingTable( leftMasks,  0,  8,  1,  0);

  buildSlidingTable(  upRightMasks, 63, -8, -1, -1);
  buildSlidingTable(  upRightMasks, 56,  1, -1, -1);
  buildSlidingTable( downLeftMasks,  0,  8,  1,  1);
  buildSlidingTable( downLeftMasks,  7, -1,  1,  1);
  buildSlidingTable(   upLeftMasks, 56,  1,  1, -1);
  buildSlidingTable(   upLeftMasks, 56, -8,  1, -1);
  buildSlidingTable(downRightMasks,  7,  8, -1,  1);
  buildSlidingTable(downRightMasks,  7, -1, -1,  1);

  buildKnightTable(knightMasks);
  buildKingTable(kingMasks);
  buildKingOuterTable(kingOuterMasks);

  buildPawnTable(pawnMasks[1],  1, false);
  buildPawnTable(pawnMasks[0], -1, false);
  buildPawnTable(pawnCaptureMasks[1],  1,  true);
  buildPawnTable(pawnCaptureMasks[0], -1,  true);

  buildPassedPawnTable(passedPawnMasks[1],   upMasks);
  buildPassedPawnTable(passedPawnMasks[0], downMasks);

  setZero(diagonalMasks);
  setZero(lineMasks);

  mergeTable(diagonalMasks,   upRightMasks,   upLeftMasks);
  mergeTable(diagonalMasks, downRightMasks, downLeftMasks);

  mergeTable(lineMasks,   upMasks,  downMasks);
  mergeTable(lineMasks, leftMasks, rightMasks);

  buildRookBishopMasks();
  buildLookUpTable( rookMasks,    rookMagics,   rookShifts,   rookStartIndex,   rookMovesLookUp,   rookSquaresBasic);
  buildLookUpTable(bishopMasks, bishopMagics, bishopShifts, bishopStartIndex, bishopMovesLookUp, bishopSquaresBasic);
}

}

