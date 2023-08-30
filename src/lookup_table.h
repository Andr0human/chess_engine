

#ifndef LOOKUP_TABLE_H
#define LOOKUP_TABLE_H

#include "types.h"


#define __abs(x) ((x >= 0) ? (x) : -(x))
#define dist_btw(x1, y1, x2, y2) (__abs((x1 - x2)) + __abs((y1 - y2)))

namespace plt
{

extern Bitboard    UpMasks[64];
extern Bitboard  DownMasks[64];
extern Bitboard  LeftMasks[64];
extern Bitboard RightMasks[64];

extern Bitboard   UpRightMasks[64];
extern Bitboard    UpLeftMasks[64];
extern Bitboard DownRightMasks[64];
extern Bitboard  DownLeftMasks[64];

extern Bitboard     LineMasks[64];	// (	 UpMask | 	DownMask | 		LeftMask | 	  RightMask)
extern Bitboard DiagonalMasks[64];	// (UpRightMask | UpLeftMask | DownRightMask | DownLeftMask)

extern Bitboard   RookMasks[64];
extern Bitboard BishopMasks[64];
extern Bitboard KnightMasks[64];
extern Bitboard   KingMasks[64];

extern Bitboard        PawnMasks[2][64];
extern Bitboard PawnCaptureMasks[2][64];
extern Bitboard  PassedPawnMasks[2][64];

extern Bitboard   RookStartIndex[64];
extern Bitboard BishopStartIndex[64];

extern Bitboard   RookMovesLookUp[106495];
extern Bitboard BishopMovesLookUp[5248];

extern Bitboard   RookMagics[64];
extern 		int   RookShifts[64];
extern Bitboard BishopMagics[64];
extern 		int BishopShifts[64];

void
Init();

}


#endif

