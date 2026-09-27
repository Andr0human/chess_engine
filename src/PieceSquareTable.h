#ifndef SQUARE_TABLE_H
#define SQUARE_TABLE_H

#include "types.h"
#include <array>

using std::array;
using ScoreTable = array<Score, SQUARE_NB>;

// (midgame, endgame) piece-square values, indexed by make_piece(c, pt) and square.
// White-relative: Black's entries are White's mirrored vertically and negated, so a
// position's piece-square score is one sum over every piece on the board.
using PieceSquareTables = array<array<TaperedScore, SQUARE_NB>, 16>;

extern const PieceSquareTables pieceSquareTable;

extern ScoreTable  loneKingLosingEndGameTable;


#endif

