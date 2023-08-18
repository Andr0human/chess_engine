

#ifndef MOVE_UTILS_H
#define MOVE_UTILS_H

#include "bitboard.h"
#include "lookup_table.h"


class MoveList
{
    private:
    Move* __begin;
    Move* __end;

    public:
    Move pMoves[120];
    Color pColor;
    
    bool cpt_only, canPromote;

    MoveList()
    : __begin(pMoves), __end(pMoves), pColor(Color::WHITE), cpt_only(false) {}

    inline void
    set(Color t_cl, bool qs_only) noexcept
    {
        pColor = t_cl;
        cpt_only = qs_only;
        canPromote = false;
        __begin = __end = pMoves;
    }

    inline void Add(Move move) noexcept
    { *__end++ = move; }

    inline bool empty() const noexcept
    { return __begin == __end; }

    inline Move* begin() const noexcept
    { return __begin; }

    inline Move* end() const noexcept
    { return __end; }

    inline uint64_t size() const noexcept
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



// Returns and removes the lsb_idx
inline Square
next_idx(uint64_t& __x)
{
    Square res = __builtin_ctzll(__x);
    __x &= __x - 1;
    return res;
}

// Prints all the info on the encoded-move
void
decode_move(Move encoded_move);






inline Move
gen_base_move(const ChessBoard& _cb, Square ip);

inline
void add_cap_moves(Square ip, Bitboard endSquares, Move base,
    const ChessBoard& _cb, MoveList& myMoves);

inline void
add_quiet_moves(Bitboard endSquares, Move base, MoveList& myMoves);

void
add_quiet_pawn_moves(Bitboard endSquares, int shift, const ChessBoard& _cb, MoveList& myMoves);

void
add_move_to_list(Square ip, Bitboard endSquares, const ChessBoard& _cb, MoveList& myMoves);










// Returns all squares attacked by all pawns from side to move
Bitboard
pawn_atk_sq(const ChessBoard& _cb, Color color);

// Returns all squares attacked by bishop on index __pos
Bitboard
bishop_atk_sq(Square __pos, Bitboard _Ap);

// Returns all squares attacked by knight on index __pos
Bitboard
knight_atk_sq(Square __pos, Bitboard _Ap);

// Returns all squares attacked by rook on index __pos
Bitboard
rook_atk_sq(Square __pos, Bitboard _Ap);

// Returns all squares attacked by queen on index __pos
Bitboard
queen_atk_sq(Square __pos, Bitboard _Ap);




/**
 * @brief Checks if player to move is in check
 * 
 * @param _cb board position
 * @return true 
 * @return false 
 */

/**
 * @brief Checks if side to move is in check
 * 
 * @param _cb 
 * @param own_king To check for king for side-to-move or opposite.
 * @return true 
 * @return false 
 */
bool
in_check(const ChessBoard& _cb);


/**
 * @brief Returns string readable move from encoded-move.
 * Invalid move leads to undefined behaviour.
 * 
 * @param move encoded-move
 * @param _cb board position
 * @return string 
 */
string
print(Move move, ChessBoard _cb);


/**
 * @brief Prints all encoded-moves in list to human-readable strings
 * 
 * @param _cb board position
 * @param myMoves Movelist for board positions.
 */
void
print(const MoveList& myMoves, const ChessBoard& _cb);




#endif


