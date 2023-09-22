

#ifndef LOOKUP_TABLE_H
#define LOOKUP_TABLE_H

#include "types.h"


#define __abs(x) ((x >= 0) ? (x) : -(x))

namespace plt
{

extern Bitboard    UpMasks[SQUARE_NB];
extern Bitboard  DownMasks[SQUARE_NB];
extern Bitboard  LeftMasks[SQUARE_NB];
extern Bitboard RightMasks[SQUARE_NB];

extern Bitboard   UpRightMasks[SQUARE_NB];
extern Bitboard    UpLeftMasks[SQUARE_NB];
extern Bitboard DownRightMasks[SQUARE_NB];
extern Bitboard  DownLeftMasks[SQUARE_NB];

extern Bitboard     LineMasks[SQUARE_NB];	// (	 UpMask | 	DownMask | 		LeftMask | 	  RightMask)
extern Bitboard DiagonalMasks[SQUARE_NB];	// (UpRightMask | UpLeftMask | DownRightMask | DownLeftMask)

extern Bitboard      RookMasks[SQUARE_NB];
extern Bitboard    BishopMasks[SQUARE_NB];
extern Bitboard    KnightMasks[SQUARE_NB];
extern Bitboard      KingMasks[SQUARE_NB];
extern Bitboard KingOuterMasks[SQUARE_NB];

extern Bitboard        PawnMasks[COLOR_NB][SQUARE_NB];
extern Bitboard PawnCaptureMasks[COLOR_NB][SQUARE_NB];
extern Bitboard  PassedPawnMasks[COLOR_NB][SQUARE_NB];

extern Bitboard   RookStartIndex[SQUARE_NB];
extern Bitboard BishopStartIndex[SQUARE_NB];

extern Bitboard*   RookMovesLookUp;
extern Bitboard* BishopMovesLookUp;

extern Bitboard   RookMagics[SQUARE_NB];
extern 		int   RookShifts[SQUARE_NB];
extern Bitboard BishopMagics[SQUARE_NB];
extern 		int BishopShifts[SQUARE_NB];

void
Init();

}


#endif

