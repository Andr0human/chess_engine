

#include "PieceSquareTable.h"

ScoreTable wpBoard = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -10, -20, -20, 0, 5, 0, 3, 10, 8, 8, 8, -3, 10, 3, 5, 2, 13, 15, 15,
    8, -3, 5, 6, 7, 15, 17, 17, 9, 7, 6, 26, 27, 31, 31, 31, 29, 27, 26, 56, 57, 61, 61, 61, 59, 57, 56,
    0, 0, 0, 0, 0, 0, 0, 0,
};

ScoreTable bpBoard = {
    0, 0, 0, 0, 0, 0, 0, 0, 56, 57, 61, 61, 61, 59, 57, 56, 26, 27, 31, 31, 31, 29, 27, 26, 6, 7, 15, 17,
    17, 9, 7, 6, 5, 2, 13, 15, 15, 8, -3, 5, 3, 10, 8, 8, 8, -3, 10, 3, 0, 0, -10, -20, -20, 0, 5, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
};

ScoreTable NBoard  = {
    -40, -15, -10, -10, -10, -15, -10, -40,
    -10,   2,   5,   5,   5,   5,   2, -10,
    -10,   7,   8,   8,   8,   8,   7, -10,
    -10,   8,  10,  10,  10,  10,   8, -10,
    -10,   8,  10,  10,  10,  10,   8, -10,
    -10,   7,   8,   8,   8,   8,   7, -10,
    -10,   2,   5,   5,   5,   5,   2, -10,
    -40, -15, -10, -10, -10, -15, -10, -40,
};

ScoreTable wBoard  = {
    -40, -5, -5, -5, -5, -5, -5, -40, 4, 10, 4, 6, 6, 4, 10, 4, 4, 4, 4, 5, 5, 4, 4, 4, 6, 7, 20, 6, 6, 20, 7,
    6, 5, 20, 4, 6, 6, 4, 20, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, -40, -5, -5, -5, -5, -5, -5, -40,
};

ScoreTable bBoard = {
    -40, -5, -5, -5, -5, -5, -5, -40, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 20, 4, 6, 6, 4, 20, 5,
    6, 7, 20, 6, 6, 20, 7, 6, 4, 4, 4, 5, 5, 4, 4, 4, 4, 10, 4, 6, 6, 4, 10, 4, -40, -5, -5, -5, -5, -5, -5, -40,
};


ScoreTable wRBoard {
    5, 10, 10, 10, 10, 10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 5, 10, 10, 10, 10,
    10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 15, 20, 20, 20, 20, 20, 20, 15, 40, 50, 50, 50, 50, 50, 50, 40, 20,
    30, 30, 30, 30, 30, 30, 20,
};

ScoreTable bRBoard {
    20, 30, 30, 30, 30, 30, 30, 20, 40, 50, 50, 50, 50, 50, 50, 40, 15, 20, 20, 20, 20, 20, 20, 15, 5, 10, 10, 10,
    10, 10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 5, 10, 10, 10, 10, 10, 10, 5, 5, 10,
    10, 10, 10, 10, 10, 5,
};


ScoreTable WhiteKingMidGameTable = {
      35,   40,   35,    0,    0,    0,   35,   35,
      35,   35,   -2,   -2,   -2,   -2,   -2,   35,
     -10,  -10,  -10,  -10,  -10,  -10,  -10,  -10,
     -40,  -40,  -50,  -50,  -50,  -50,  -40,  -40,
     -70,  -70,  -85,  -85,  -85,  -85,  -70,  -70,
     -90,  -90, -115, -115, -115, -115,  -90,  -90,
    -150, -165, -165, -165, -165, -165, -165, -150,
    -200, -200, -200, -200, -200, -200, -200, -200,
};


ScoreTable BlackKingMidGameTable = {
    -200, -200, -200, -200, -200, -200, -200, -200,
    -150, -165, -165, -165, -165, -165, -165, -150,
     -90,  -90, -115, -115, -115, -115,  -90,  -90,
     -70,  -70,  -85,  -85,  -85,  -85,  -70,  -70,
     -40,  -40,  -50,  -50,  -50,  -50,  -40,  -40,
     -10,  -10,  -10,  -10,  -10,  -10,  -10,  -10,
      35,   35,   -2,   -2,   -2,   -2,   -2,   35,
      35,   40,   35,    0,    0,    0,   35,   35,
};


ScoreTable KingEndGameTable = {
    -10,  -8,  -4,   1,   1,  -4,  -8, -10,
     -8,   4,   8,  14,  14,   8,   4,  -8,
     -4,   8,  18,  24,  24,  18,   8,  -4,
      1,  14,  24,  31,  31,  24,  14,   1,
      1,  14,  24,  31,  31,  24,  14,   1,
     -4,   8,  18,  24,  24,  18,   8,  -4,
     -8,   4,   8,  14,  14,   8,   4,  -8,
    -10,  -8,  -4,   1,   1,  -4,  -8, -10,
};


ScoreTable LoneKingWinningEndGameTable = {
    -126, -85, -54, -23, -23, -54, -85, -126,
     -85, -21,  12,  39,  39,  12, -21,  -85,
     -54,  12,  58,  85,  85,  58,  12,  -54,
     -23,  39,  85, 115, 115,  85,  39,  -23,
     -23,  39,  85, 115, 115,  85,  39,  -23,
     -54,  12,  58,  85,  85,  58,  12,  -54,
     -85, -21,  12,  39,  39,  12, -21,  -85,
    -126, -85, -54, -23, -23, -54, -85, -126,
};

ScoreTable LoneKingLosingEndGameTable = {
    -496, -327, -327, -327, -327, -327, -327, -496,
    -327,    0,    0,    0,    0,    0,    0, -327,
    -327,    0,    0,    0,    0,    0,    0, -327,
    -327,    0,    0,    0,    0,    0,    0, -327,
    -327,    0,    0,    0,    0,    0,    0, -327,
    -327,    0,    0,    0,    0,    0,    0, -327,
    -327,    0,    0,    0,    0,    0,    0, -327,
    -496, -327, -327, -327, -327, -327, -327, -496,
};

