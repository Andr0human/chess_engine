

#ifndef SQUARE_TABLE_H
#define SQUARE_TABLE_H

#include "types.h"

///      Square Tables for Pieces      ////


extern Score wpBoard[SQUARE_NB];
extern Score bpBoard[SQUARE_NB];
extern Score  NBoard[SQUARE_NB];
extern Score  wBoard[SQUARE_NB];
extern Score  bBoard[SQUARE_NB];

extern Score wRBoard[SQUARE_NB];
extern Score bRBoard[SQUARE_NB];

extern Score WhiteKingMidGameTable[SQUARE_NB];
extern Score BlackKingMidGameTable[SQUARE_NB];
extern Score      KingEndGameTable[SQUARE_NB];

extern Score LongKingWinningEndGameTable[SQUARE_NB];
extern Score  LoneKingLosingEndGameTable[SQUARE_NB];


#endif

