

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

// The endgame tables are Texel-tuned (`elsa tune pst --all`, all three datasets
// combined) with the weights held fixed, files folded so each table is left-right
// symmetric. Each table but the king's has a zero phase-weighted mean over the data,
// so it moves pieces around without changing their material value.

static constexpr ScoreTable pawnEg = {
    0,   0,   0,   0,   0,   0,   0,   0,
  -10,   2,   7,  14,  14,   7,   2, -10,
   -8,  -2,  -1,  -3,  -3,  -1,  -2,  -8,
   -5,   7,  -4,  -7,  -7,  -4,   7,  -5,
   15,  19,  10,   4,   4,  10,  19,  15,
   43,  51,  38,  25,  25,  38,  51,  43,
   54,  56,  50,  35,  35,  50,  56,  54,
    0,   0,   0,   0,   0,   0,   0,   0,
};

static constexpr ScoreTable knightEg = {
   -22,  -30,  -11,  -10,  -10,  -11,  -30,  -22,
     3,  -20,  -11,   -3,   -3,  -11,  -20,    3,
   -15,   -6,  -10,   11,   11,  -10,   -6,  -15,
     6,    5,   12,   20,   20,   12,    5,    6,
     5,   13,   25,   31,   31,   25,   13,    5,
   -11,    3,   15,   22,   22,   15,    3,  -11,
   -31,  -15,   -1,    1,    1,   -1,  -15,  -31,
  -104,  -37,  -28,  -17,  -17,  -28,  -37, -104,
};

static constexpr ScoreTable bishopEg = {
    7,   7, -11,  -6,  -6, -11,   7,   7,
   -1,   6,   1,  -3,  -3,   1,   6,  -1,
   -1,   5,   8,  10,  10,   8,   5,  -1,
  -10,  -2,  -9,  10,  10,  -9,  -2, -10,
   -8,  -7,   4,  12,  12,   4,  -7,  -8,
    4,   0,   6,   1,   1,   6,   0,   4,
  -30, -12, -10,  -7,  -7, -10, -12, -30,
   -9,  -8, -13, -10, -10, -13,  -8,  -9,
};

static constexpr ScoreTable rookEg = {
   -5,  -1,   7,   3,   3,   7,  -1,  -5,
  -11,  -7,  -9,  -9,  -9,  -9,  -7, -11,
   -7, -10, -12, -10, -10, -12, -10,  -7,
   -4,  -6,  -6,  -4,  -4,  -6,  -6,  -4,
    3,   0,   3,   0,   0,   3,   0,   3,
    1,   4,   3,   1,   1,   3,   4,   1,
   -1,  -1,   5,   6,   6,   5,  -1,  -1,
   13,   9,  12,   9,   9,  12,   9,  13,
};

static constexpr ScoreTable queenEg = {
  -11, -25, -20,   0,   0, -20, -25, -11,
  -10, -12,  -8,  -2,  -2,  -8, -12, -10,
   -9,  -2,   2,  -9,  -9,   2,  -2,  -9,
   -4,   3,  -3,   5,   5,  -3,   3,  -4,
   -1,   4,   9,  12,  12,   9,   4,  -1,
    6,   0,  23,  24,  24,  23,   0,   6,
  -16, -19,  13,  23,  23,  13, -19, -16,
  -11,   1,  18,  23,  23,  18,   1, -11,
};

static constexpr ScoreTable kingEg = {
  -22, -18, -21, -23, -23, -21, -18, -22,
  -18,  -9,  -8, -12, -12,  -8,  -9, -18,
  -13, -10,  -9,  -5,  -5,  -9, -10, -13,
   -6,   4,   8,  11,  11,   8,   4,  -6,
   13,  27,  32,  30,  30,  32,  27,  13,
   27,  48,  49,  47,  47,  49,  48,  27,
   39,  59,  57,  52,  52,  57,  59,  39,
    4,  35,  38,  40,  40,  38,  35,   4,
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

// The queen has no midgame table yet.
static constexpr PieceSquareTables
buildPieceSquareTable()
{
  PieceSquareTables table = {};

  addPiece(table, PAWN  , pawnMg  , pawnEg  );
  addPiece(table, BISHOP, bishopMg, bishopEg);
  addPiece(table, KNIGHT, knightMg, knightEg);
  addPiece(table, ROOK  , rookMg  , rookEg  );
  addPiece(table, QUEEN , noTable , queenEg );
  addPiece(table, KING  , kingMg  , kingEg  );

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

