

#include "PieceSquareTable.h"

int wpBoard[64] = { 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -10, -20, -20, 0, 5, 0, 3, 10, 8, 8, 8, -3, 10, 3, 5, 2, 13, 15, 15,
    8, -3, 5, 6, 7, 15, 17, 17, 9, 7, 6, 26, 27, 31, 31, 31, 29, 27, 26, 56, 57, 61, 61, 61, 59, 57, 56,
    0, 0, 0, 0, 0, 0, 0, 0,
};

int bpBoard[64] = { 
    0, 0, 0, 0, 0, 0, 0, 0, 56, 57, 61, 61, 61, 59, 57, 56, 26, 27, 31, 31, 31, 29, 27, 26, 6, 7, 15, 17,
    17, 9, 7, 6, 5, 2, 13, 15, 15, 8, -3, 5, 3, 10, 8, 8, 8, -3, 10, 3, 0, 0, -10, -20, -20, 0, 5, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
};

int NBoard[64] = { 
    -40, -15, -10, -10, -10, -15, -10, -40, -10, 2, 5, 5, 5, 5, 2, -10, -10, 7, 8, 8, 8, 8, 7, -10, -10,
    8, 10, 10, 10, 10, 8, -10, -10, 8, 10, 10, 10, 10, 8, -10, -10, 7, 8, 8, 8, 8, 7, -10, -10, 2, 5, 5,
    5, 5, 2, -10, -40, -10, -10, -10, -10, -10, -10, -40,
};

int wBoard[64] = { 
    -40, -5, -5, -5, -5, -5, -5, -40, 4, 10, 4, 6, 6, 4, 10, 4, 4, 4, 4, 5, 5, 4, 4, 4, 6, 7, 20, 6, 6, 20, 7,
    6, 5, 20, 4, 6, 6, 4, 20, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, -40, -5, -5, -5, -5, -5, -5, -40,
};

int bBoard[64] = {
    -40, -5, -5, -5, -5, -5, -5, -40, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 20, 4, 6, 6, 4, 20, 5,
    6, 7, 20, 6, 6, 20, 7, 6, 4, 4, 4, 5, 5, 4, 4, 4, 4, 10, 4, 6, 6, 4, 10, 4, -40, -5, -5, -5, -5, -5, -5, -40,
};


int wRBoard[64] = {
    5, 10, 10, 10, 10, 10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 5, 10, 10, 10, 10, 
    10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 15, 20, 20, 20, 20, 20, 20, 15, 40, 50, 50, 50, 50, 50, 50, 40, 20, 
    30, 30, 30, 30, 30, 30, 20,
};

int bRBoard[64] = {
    20, 30, 30, 30, 30, 30, 30, 20, 40, 50, 50, 50, 50, 50, 50, 40, 15, 20, 20, 20, 20, 20, 20, 15, 5, 10, 10, 10, 
    10, 10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 5, 10, 
    10, 10, 10, 10, 10, 5,
};


Score WhiteKingMidGameTable[64] = {
      35,   40,   35,    0,    0,    0,   35,   35, 
      35,   35,   -2,   -2,   -2,   -2,   -2,   35, 
     -10,  -10,  -10,  -10,  -10,  -10,  -10,  -10, 
     -40,  -40,  -50,  -50,  -50,  -50,  -40,  -40, 
     -70,  -70,  -85,  -85,  -85,  -85,  -70,  -70, 
     -90,  -90, -115, -115, -115, -115,  -90,  -90, 
    -150, -165, -165, -165, -165, -165, -165, -150, 
    -200, -200, -200, -200, -200, -200, -200, -200,
};


Score BlackKingMidGameTable[64] = {
     200, -200, -200, -200, -200, -200, -200, -200,
    -150, -165, -165, -165, -165, -165, -165, -150, 
     -90,  -90, -115, -115, -115, -115,  -90,  -90,
     -90,  -90, -115, -115, -115, -115,  -90,  -90,
     -70,  -70,  -85,  -85,  -85,  -85,  -70,  -70,
     -10,  -10,  -10,  -10,  -10,  -10,  -10,  -10,
      30,   35,   -2,   -2,   -2,   -2,   -2,   35,
      35,   40,   35,    0,    0,    0,   35,   35,
};


Score KingEndGameTable[64] = {
    -10,  -8,  -4,   1,   1,  -4,  -8, -10, 
     -8,   4,   8,  14,  14,   8,   4,  -8, 
     -4,   8,  18,  24,  24,  18,   8,  -4, 
      1,  14,  24,  31,  31,  24,  14,   1, 
      1,  14,  24,  31,  31,  24,  14,   1, 
     -4,   8,  18,  24,  24,  18,   8,  -4, 
     -8,   4,   8,  14,  14,   8,   4,  -8, 
    -10,  -8,  -4,   1,   1,  -4,  -8, -10, 
};


Score LoneKingEndGameTable[64] = {
    -30, -30, -30, -30, -30, -30, -30, -30,
    -30, -12, -12, -12, -12, -12, -12, -30, 
    -30, -12,   4,   4,   4,   4, -12, -30, 
    -30, -12,   4,  20,  20,   4, -12, -30, 
    -30, -12,   4,  20,  20,   4, -12, -30, 
    -30, -12,   4,   4,   4,   4, -12, -30, 
    -30, -12, -12, -12, -12, -12, -12, -30, 
    -30, -30, -30, -30, -30, -30, -30, -30,
};

