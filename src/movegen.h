

#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "bitboard.h"
#include "lookup_table.h"
#include "move_utils.h"

/*

***********************  MOVE STRUCTURE  ***********************

    (check) (type) (color) (pp) (pFp) (pIp)  (fp)    (ip)
       0      00      0     00   000   000  000000  000000

* check (1 bit) - indicates whether the move places the opponent's king in check [0: No-check, 1: Check]
* type (2 bits) - type of move [00: Quiet, 01: Castle, 10: Captures, 11: Promotion]
* color (1 bit) - color of the piece making the move [0: Black, 1: White]
* pp   (2 bits) - indicates the type of piece at pawn promotion [0: Bishop, 1: Knight, 2: Rook, 3: Queen]
* ip   (6 bits) - initial square of the piece
* fp   (6 bits) - destination square of the piece
* pIp  (3 bits) - pieceType at initial square
* pFp  (3 bits) - pieceType at destination square

*/


// Checks if move is valid for given position
bool IsLegalMoveForPosition(Move move, ChessBoard& pos);

// Returns true if board position has at least one legal move.
bool LegalMovesPresent(ChessBoard& _cb);

/**
 * @brief Returns a list of all the legal moves in current position.
 *
 * @param _cb ChessBoard
 * @param qSearch
 * @return MoveList
 */
MoveList GenerateMoves(ChessBoard& pos, bool generateChecksData=false);


bool
CapturesExistInPosition(const ChessBoard& pos);

bool
QueenTrapped(const ChessBoard& pos, Bitboard enemyAttackedSquares);

Square
GetSmallestAttacker(const ChessBoard& pos, const Square square, Color side, Bitboard removedPieces);


#endif
