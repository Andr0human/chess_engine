

#ifndef LOOKUP_TABLE_H
#define LOOKUP_TABLE_H

#include <cstdint>


#define __abs(x) ((x >= 0) ? (x) : -(x))
#define dist_btw(x1, y1, x2, y2) (__abs((x1 - x2)) + __abs((y1 - y2)))

namespace plt
{

extern uint64_t    UpMasks[64];
extern uint64_t  DownMasks[64];
extern uint64_t  LeftMasks[64];
extern uint64_t RightMasks[64];

extern uint64_t   UpRightMasks[64];
extern uint64_t    UpLeftMasks[64];
extern uint64_t DownRightMasks[64];
extern uint64_t  DownLeftMasks[64];

extern uint64_t     LineMasks[64];	// (	 UpMask | 	DownMask | 		LeftMask | 	  RightMask)
extern uint64_t DiagonalMasks[64];	// (UpRightMask | UpLeftMask | DownRightMask | DownLeftMask)

extern uint64_t   RookMasks[64];
extern uint64_t BishopMasks[64];
extern uint64_t KnightMasks[64];
extern uint64_t   KingMasks[64];

extern uint64_t        PawnMasks[2][64];
extern uint64_t PawnCaptureMasks[2][64];
extern uint64_t  PassedPawnMasks[2][64];

extern uint64_t   RookStartIndex[64];
extern uint64_t BishopStartIndex[64];

extern uint64_t   RookMovesLookUp[106495];
extern uint64_t BishopMovesLookUp[5248];

extern uint64_t   RookMagics[64];
extern 		int   RookShifts[64];
extern uint64_t BishopMagics[64];
extern 		int BishopShifts[64];

void
init();

}


#endif

