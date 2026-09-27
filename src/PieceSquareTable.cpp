

#include "PieceSquareTable.h"

// White's tables, indexed by square with a1 = 0, so each one reads upside down: the
// first row is rank 1. Black's entries are derived from them in buildPieceSquareTable().

static constexpr ScoreTable pawnMg = {
   0,  0,   0,   0,   0,  0,  0,  0,
   0,  0, -10, -20, -20,  0,  5,  0,
   3, 10,   8,   8,   8, -3, 10,  3,
   5,  2,  13,  15,  15,  8, -3,  5,
   6,  7,  15,  17,  17,  9,  7,  6,
  26, 27,  31,  31,  31, 29, 27, 26,
  56, 57,  61,  61,  61, 59, 57, 56,
   0,  0,   0,   0,   0,  0,  0,  0,
};

static constexpr ScoreTable knightMg = {
  -40, -15, -10, -10, -10, -15, -10, -40,
  -10,   2,   5,   5,   5,   5,   2, -10,
  -10,   7,   8,   8,   8,   8,   7, -10,
  -10,   8,  10,  10,  10,  10,   8, -10,
  -10,   8,  10,  10,  10,  10,   8, -10,
  -10,   7,   8,   8,   8,   8,   7, -10,
  -10,   2,   5,   5,   5,   5,   2, -10,
  -40, -15, -10, -10, -10, -15, -10, -40,
};

static constexpr ScoreTable bishopMg = {
  -40, -5, -5, -5, -5, -5, -5, -40,
    4, 10,  4,  6,  6,  4, 10,   4,
    4,  4,  4,  5,  5,  4,  4,   4,
    6,  7, 20,  6,  6, 20,  7,   6,
    5, 20,  4,  6,  6,  4, 20,   5,
    4,  4,  4,  4,  4,  4,  4,   4,
    4,  4,  4,  4,  4,  4,  4,   4,
  -40, -5, -5, -5, -5, -5, -5, -40,
};

static constexpr ScoreTable rookMg = {
   5, 10, 10, 10, 10, 10, 10,  5,
   5, 10, 10, 10, 10, 10, 10,  5,
   5, 10, 10, 10, 10, 10, 10,  5,
   5, 10, 10, 10, 10, 10, 10,  5,
   5, 10, 10, 10, 10, 10, 10,  5,
  15, 20, 20, 20, 20, 20, 20, 15,
  40, 50, 50, 50, 50, 50, 50, 40,
  20, 30, 30, 30, 30, 30, 30, 20,
};

static constexpr ScoreTable kingMg = {
    35,   40,   35,    0,    0,    0,   35,   35,
    35,   35,   -2,   -2,   -2,   -2,   -2,   35,
   -10,  -10,  -10,  -10,  -10,  -10,  -10,  -10,
   -40,  -40,  -50,  -50,  -50,  -50,  -40,  -40,
   -70,  -70,  -85,  -85,  -85,  -85,  -70,  -70,
   -90,  -90, -115, -115, -115, -115,  -90,  -90,
  -150, -165, -165, -165, -165, -165, -165, -150,
  -200, -200, -200, -200, -200, -200, -200, -200,
};

static constexpr ScoreTable kingEg = {
  -10,  -8,  -4,   1,   1,  -4,  -8, -10,
   -8,   4,   8,  14,  14,   8,   4,  -8,
   -4,   8,  18,  24,  24,  18,   8,  -4,
    1,  14,  24,  31,  31,  24,  14,   1,
    1,  14,  24,  31,  31,  24,  14,   1,
   -4,   8,  18,  24,  24,  18,   8,  -4,
   -8,   4,   8,  14,  14,   8,   4,  -8,
  -10,  -8,  -4,   1,   1,  -4,  -8, -10,
};

static constexpr ScoreTable noTable = {};

static constexpr void
addPiece(PieceSquareTables& table, PieceType pt, const ScoreTable& mg, const ScoreTable& eg)
{
  for (int sq = 0; sq < SQUARE_NB; sq++)
  {
    const TaperedScore value = {mg[sq], eg[sq]};

    table[make_piece(WHITE, pt)][sq]      =  value;
    table[make_piece(BLACK, pt)][sq ^ 56] = -value;
  }
}

// The queen has no table yet, and only the king has an endgame table.
static constexpr PieceSquareTables
buildPieceSquareTable()
{
  PieceSquareTables table = {};

  addPiece(table, PAWN  , pawnMg  , noTable);
  addPiece(table, BISHOP, bishopMg, noTable);
  addPiece(table, KNIGHT, knightMg, noTable);
  addPiece(table, ROOK  , rookMg  , noTable);
  addPiece(table, KING  , kingMg  , kingEg );

  return table;
}

constexpr PieceSquareTables pieceSquareTable = buildPieceSquareTable();

ScoreTable loneKingLosingEndGameTable = {
  -296, -216, -144, -96, -96, -144, -216, -296,
  -216,    0,    0,   0,   0,    0,    0, -216,
  -144,    0,    0,   0,   0,    0,    0, -144,
   -96,    0,    0,   0,   0,    0,    0,  -96,
   -96,    0,    0,   0,   0,    0,    0,  -96,
  -144,    0,    0,   0,   0,    0,    0, -144,
  -216,    0,    0,   0,   0,    0,    0, -216,
  -296, -216, -144, -96, -96, -144, -216, -296,
};

