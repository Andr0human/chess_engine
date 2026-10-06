

#include "PieceSquareTable.h"

// White's tables, indexed by square with a1 = 0, so each one reads upside down: the
// first row is rank 1. Black's entries are derived from them in buildPieceSquareTable().
//
// All twelve tables are Texel-tuned on the three datasets combined, with the weights held
// fixed. The tuner keeps each table's phase-weighted mean over the data where it started
// (except the king's), so it moves values between squares without changing material value.

// The midgame tables were fitted square by square (`elsa tune pst --all tables mg unfold mg`)
// with the endgame tables held fixed, so they are not left-right symmetric.

static constexpr ScoreTable pawnMg = {
    0,   0,   0,   0,   0,   0,   0,   0,
   -5, -11, -16, -19,  -6,  16,  24,  -7,
   -7,  -8,  -4,   1,  12,   8,  20,   1,
   -6,  -7,   1,  16,  17,  13,   1,  -9,
    4,   3,   9,  14,  30,  23,  15,   3,
   17,  19,  39,  36,  42,  65,  34,  14,
   83,  70,  74,  73,  68,  26,  -5, -68,
    0,   0,   0,   0,   0,   0,   0,   0,
};

static constexpr ScoreTable knightMg = {
   -47,  -10,  -26,  -12,  -10,    7,   -8,  -36,
   -26,  -12,   -7,    7,    4,   10,    6,    7,
   -20,   -5,   -5,    4,   15,    3,   15,   -4,
    -7,    0,   10,    8,   19,   10,   19,    1,
     4,    8,   14,   36,   14,   41,   16,   31,
    -6,   26,   21,   34,   46,   54,   29,    7,
   -30,   -3,   17,   29,   11,   37,   -5,  -11,
  -129,  -50,  -18,  -41,   -5, -110,  -54, -104,
};

static constexpr ScoreTable bishopMg = {
  -14,  14,  -2,  -3,  -6,  -7,  -5,  -6,
   16,   6,  15,  -2,   7,  15,  27,  10,
    4,   8,   4,   3,   3,   6,   3,  16,
   -1,   0,   4,  14,  11,  -2,   0,   2,
   -3,   6,   7,  20,  11,   7,   4,  -6,
   -6,  13,   8,  16,   3,  29,   8,  17,
  -20,  -2,  -8,  -2,  -2, -16, -29, -38,
  -59, -38, -20, -49, -66, -76, -51, -98,
};

static constexpr ScoreTable rookMg = {
   10,   8,   7,  11,  13,  13,  15,   0,
   -6,  -3,   5,   4,   2,   8,  20,  -4,
   -7,  -5,  -2,   0,   0,   0,  17,   3,
   -3,  -5,  -3,   1,  -5, -12,   2,  -9,
    5,   9,   9,   9,  -4,   3,   3,   1,
   11,  26,  18,  16,  20,  24,  22,  15,
   27,  29,  47,  53,  31,  24,  17,  29,
   11,  22,  24,  19,   1,   0,   8,  20,
};

static constexpr ScoreTable queenMg = {
    8,  -2,   2,   7,  -1,   2,  10,   6,
    0,   0,   3,   6,   5,  14,  16,  28,
  -12,  -7,  -8,  -7,   0,  -1,  12,   9,
  -13, -17, -16, -12,  -9,  -5,   1,   9,
  -26, -20, -23, -23, -11,  -2,   0,  11,
  -26, -21, -23, -16,  -9,  25,  13,  31,
  -31, -33, -31, -23, -17,  -3,   5,  53,
  -40, -20,  -9,  -6,  -5,  11,  23,   3,
};

static constexpr ScoreTable kingMg = {
    50,   64,   45,  -26,    1,  -12,   37,   41,
    66,   17,    2,  -23,  -24,   -9,   10,   27,
     8,    8,  -18,  -30,  -30,  -35,  -20,  -18,
   -23,  -11,  -21,  -41,  -57,  -58,  -53,  -63,
   -51,  -47,  -58,  -71,  -68,  -71,  -64,  -79,
   -77,  -44,  -98,  -71,  -67,  -43,  -51,  -65,
  -122,  -98, -110,  -98,  -81,  -76,  -93,  -95,
   -76, -130, -115, -130, -118,  -90,  -66,  -63,
};

// The endgame tables were fitted first (`elsa tune pst --all`) with files folded, so each
// is left-right symmetric and has a zero mean.

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

static constexpr PieceSquareTables
buildPieceSquareTable()
{
  PieceSquareTables table = {};

  addPiece(table, PAWN  , pawnMg  , pawnEg  );
  addPiece(table, BISHOP, bishopMg, bishopEg);
  addPiece(table, KNIGHT, knightMg, knightEg);
  addPiece(table, ROOK  , rookMg  , rookEg  );
  addPiece(table, QUEEN , queenMg , queenEg );
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

