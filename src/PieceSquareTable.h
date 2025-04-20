

#ifndef SQUARE_TABLE_H
#define SQUARE_TABLE_H

#include "types.h"
#include <array>

using std::array;
using ScoreTable = array<Score, SQUARE_NB>;

/***      Square Tables for Pieces      ***/

extern ScoreTable wpBoard;
extern ScoreTable bpBoard;
extern ScoreTable  NBoard;
extern ScoreTable  wBoard;
extern ScoreTable  bBoard;

extern ScoreTable wRBoard;
extern ScoreTable bRBoard;

extern ScoreTable whiteKingMidGameTable;
extern ScoreTable blackKingMidGameTable;
extern ScoreTable      kingEndGameTable;

extern ScoreTable  loneKingLosingEndGameTable;


#endif

