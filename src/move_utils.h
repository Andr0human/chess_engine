
#ifndef MOVE_UTILS_H
#define MOVE_UTILS_H

#include "bitboard.h"


void
decodeMove(Move encodedMove);

// Invalid move leads to undefined behaviour.
string
printMove(Move move, ChessBoard pos);

// UCI long-algebraic string for an encoded move (e.g. "e2e4", "e7e8q",
// "e1g1" for kingside castle).
string
moveToUci(Move move);

// Returns NULL_MOVE if the string does not correspond to any legal move.
Move
moveFromUci(const string& uci, const ChessBoard& pos);

template <Color cMy>
bool
inCheck(const ChessBoard& pos);

#endif


