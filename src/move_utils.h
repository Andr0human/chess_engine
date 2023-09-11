

#ifndef MOVE_UTILS_H
#define MOVE_UTILS_H

#include <iomanip>
#include "bitboard.h"
#include "lookup_table.h"


class MoveList
{
    // Note : Need to create a copy constructor before copying this class.
    // __begin, __end address pointer needs to fix for copying.
    private:
    Move* __begin;
    Move* __end;

    public:
    // Stores all moves for current position
    Move pMoves[MAX_MOVES];

    // Active side color
    Color color;

    // Generate moves for Quisense Search
    bool qsSearch;

    // Array to store squares that give check to the enemy king
    // {Pawn, Bishop, Knight, Rook, Queen}
    // Index 1: Squares where a bishop can give check to the enemy king
    Bitboard squaresThatCheckEnemyKing[5];

    // Bitboard of initial squares from which a moving piece
    // could potentially give a discovered check to the opponent's king.
    Bitboard discoverCheckSquares;

    // Bitboard of squares that have pinned pieces on them
    Bitboard pinnedPiecesSquares;


    MoveList()
    : __begin(pMoves), __end(pMoves), color(WHITE), qsSearch(false) {}

    MoveList(Color c, bool qs = false)
    : __begin(pMoves), __end(pMoves), color(c), qsSearch(qs) {}

    inline void
    Add(Move move) noexcept
    { *__end++ = move; }

    inline bool
    empty() const noexcept
    { return __begin == __end; }

    inline Move*
    begin() const noexcept
    { return __begin; }

    inline Move*
    end() const noexcept
    { return __end; }

    inline uint64_t
    size() const noexcept
    { return __end - __begin; }
};



/**
 * @brief CheckData stores squares for which if pieces lands
 *        results in a check for the enemy king.
 */
class CheckData
{
    public:
    // (Queen + Rook) landing results in a check
    Bitboard LineSquares;

    // (Queen + Bishop) landing results in a check
    Bitboard DiagonalSquares;

    // Knight landing results in a check
    Bitboard KnightSquares;

    // Pawns landing results in a check
    Bitboard PawnSquares;


    //TODO: Add Code for this.
    Bitboard KingSquares;

    inline CheckData() {}

    inline CheckData(Bitboard __r, Bitboard __b, Bitboard __k, Bitboard __p)
    : LineSquares(__r), DiagonalSquares(__b), KnightSquares(__k), PawnSquares(__p) {}

    inline Bitboard
    squares_for_piece(const PieceType piece)
    {
        if (piece == PAWN)
            return PawnSquares;
        if (piece == BISHOP)
            return DiagonalSquares;
        if (piece == KNIGHT)
            return KnightSquares;
        if (piece == ROOK)
            return LineSquares;
        return (LineSquares | DiagonalSquares);
    }
};



// Returns and removes the LsbIndex
inline Square
NextSquare(uint64_t& __x)
{
    Square res = Square(__builtin_ctzll(__x));
    __x &= __x - 1;
    return res;
}

// Prints all the info on the encoded-move
void
DecodeMove(Move encoded_move);








// Returns all squares attacked by all pawns from side to move
Bitboard
PawnAttackSquares(const ChessBoard& _cb, Color color);

// Returns all squares attacked by bishop on index __pos
Bitboard
BishopAttackSquares(Square __pos, Bitboard _Ap);

// Returns all squares attacked by knight on index __pos
Bitboard
KnightAttackSquares(Square __pos, Bitboard _Ap);

// Returns all squares attacked by rook on index __pos
Bitboard
RookAttackSquares(Square __pos, Bitboard _Ap);

// Returns all squares attacked by queen on index __pos
Bitboard
QueenAttackSquares(Square __pos, Bitboard _Ap);



/**
 * @brief Checks if the king of the active side is in check.
 * 
 * @param ChessBoard position
**/
bool
InCheck(const ChessBoard& _cb);


/**
 * @brief Returns string readable move from encoded-move.
 * Invalid move leads to undefined behaviour.
 * 
 * @param move encoded-move
 * @param _cb board position
 * @return string 
 */
string
PrintMove(Move move, ChessBoard _cb);


/**
 * @brief Prints all encoded-moves in list to human-readable strings
 * 
 * @param _cb board position
 * @param myMoves Movelist for board positions.
 */
void
PrintMovelist(MoveList myMoves, ChessBoard _cb);


bool
MoveGivesCheck(Move move, ChessBoard& pos, const MoveList& myMoves);

#endif


