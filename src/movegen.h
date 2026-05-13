

#ifndef MOVEGEN_H
#define MOVEGEN_H

#include <utility>
#include "bitboard.h"
#include "lookup_table.h"
#include "movelist.h"

/*

***********************  MOVE STRUCTURE  ***********************

    (check) (color) (type) (pp) (pFp) (pIp)  (fp)    (ip)
       0       0      00    00   000   000  000000  000000

* check (1 bit) (<< 23) - indicates whether the move places the opponent's king in check [0: No-check, 1: Check]
* color (1 bit) (<< 22) - color of the piece making the move [0: Black, 1: White]
* type (2 bits) (<< 20) - type of move [00: Quiet, 01: Captures, 10: Promotion]
* pp   (2 bits) (<< 18) - indicates the type of piece at pawn promotion [0: Bishop, 1: Knight, 2: Rook, 3: Queen]
* ip   (6 bits) (<<  0) - initial square of the piece
* fp   (6 bits) (<<  6) - destination square of the piece
* pIp  (3 bits) (<< 12) - pieceType at initial square
* pFp  (3 bits) (<< 15) - pieceType at destination square

*/


// Checks if move is valid for given position
bool
isLegalMoveForPosition(Move move, const ChessBoard& pos);

// Same contract as isLegalMoveForPosition, but skips full move generation:
// only computes the work needed for the moving piece (geometric reachability
// + post-move king-safety via super-piece scan from king square).
bool
isLegalMoveForPosition_V2(Move move, const ChessBoard& pos);

/**
 * @brief Runs one stage of move generation on the given MoveList.
 *
 * @param pos ChessBoard
 * @param myMoves staging buffer (in/out)
 */
template <MoveGenStage stage>
void
stagedGenerateMoves(const ChessBoard& pos, MoveList& myMoves);

/**
 * @brief Returns a list of all the legal moves in current position.
 *
 * Thin wrapper that runs GEN_METADATA + GEN_MOVES (+ GEN_CHECKS when asked).
 *
 * @param pos ChessBoard
 * @param generateChecksData bool
 */
MoveList
generateMoves(const ChessBoard& pos, bool generateChecksData=false);

bool
pieceTrapped(const ChessBoard& pos, Bitboard myAttackedBB, Bitboard enemyAttackedBB);

Square
getSmallestAttacker(const ChessBoard& pos, const Square square, Color side, Bitboard removedPieces);


#endif
