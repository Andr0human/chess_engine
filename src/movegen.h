

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

/**
 * @brief Cheap legality check for a candidate move (e.g. a transposition-table
 * best move) against a position whose metadata has already been computed.
 *
 * @param move  24-bit packed move (only bits 0..19 — ip/fp/pIp/pFp/promo — are
 *              consulted; the type/colour/check bits are ignored).
 * @param pos   the position the move would be played in.
 * @param meta  a MoveList already filled by stagedGenerateMoves<GEN_METADATA>
 *              (needs checkers / enemyAttackedSquares / legalSquaresMaskInCheck).
 *
 * Deliberately biased toward returning false: a false negative just costs the
 * caller its fast path, a false positive would feed an illegal move to makeMove.
 */
bool
isHashMoveLegal(Move move, const ChessBoard& pos, const MoveList& meta);

/**
 * @brief Micro-benchmark hook: times the legacy 8-direction pinnedRayDest vs.
 * the rayDirection-dispatched pinnedRayDest_fast on every non-king own-side
 * piece in `pos`, repeated `iters` times. Returns {slow_seconds, fast_seconds}.
 *
 * Single-position numbers are noisy; callers should sum across many positions.
 */
std::pair<double, double>
benchmarkPinnedRayDest(const ChessBoard& pos, int iters);

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
