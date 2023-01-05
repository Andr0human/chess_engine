

#ifndef MOVE_UTILS_H
#define MOVE_UTILS_H

#include "bitboard.h"
#include "lookup_table.h"

#define WHITE 8
#define BLACK 0
#define OWN ((_cb.color) << 3)
#define EMY ((_cb.color ^ 1) << 3)

#define PAWN(x)   _cb.Pieces[x + 1]
#define BISHOP(x) _cb.Pieces[x + 2]
#define KNIGHT(x) _cb.Pieces[x + 3]
#define ROOK(x)   _cb.Pieces[x + 4]
#define QUEEN(x)  _cb.Pieces[x + 5]
#define KING(x)   _cb.Pieces[x + 6]
#define ALL(x)    _cb.Pieces[x + 7]
#define ALL_BOTH  (_cb.Pieces[15] | _cb.Pieces[7])



enum board: uint64_t
{
    Rank1 = 255ULL, Rank2 = Rank1 << 8, Rank3 = Rank1 << 16, Rank4 = Rank1 << 24,
    Rank5 = Rank1 << 32, Rank6 = Rank1 << 40, Rank7 = Rank1 << 48, Rank8 = Rank1 << 56,
    Rank18 = Rank1 | Rank8,

    FileA = 72340172838076673ULL, FileB = FileA << 1, FileC = FileA << 2, FileD = FileA << 3,
    FileE = FileA << 4, FileF = FileA << 5, FileG = FileA << 6, FileH = FileA << 7,

    dg_row  = 72624976668147840ULL,
    adg_row = 9241421688590303745ULL, 

    AllSquares = ~(0ULL), WhiteSquares = 6172840429334713770ULL,
    BlackSquares = AllSquares ^ WhiteSquares,
    
    QueenSide = FileA | FileB | FileC | FileD,
    KingSide  = FileE | FileF | FileG | FileH,
    WhiteSide = Rank1 | Rank2 | Rank3 | Rank4,
    BlackSide = Rank5 | Rank6 | Rank7 | Rank8,

    RightAttkingPawns = AllSquares ^ (Rank18 | FileH),
    LeftAttkingPawns  = AllSquares ^ (Rank18 | FileA),

    CentralSquare = (Rank4 | Rank5) & (FileD | FileE),
    SemiCentralSquare = (Rank3 | Rank4 | Rank5 | Rank6) & (FileC | FileD | FileE | FileF),
    OuterSquare = AllSquares ^ SemiCentralSquare,
};


class MoveList
{
    private:
    MoveType* __begin;
    MoveType* __end;

    public:
    MoveType pMoves[120];
    int pColor;
    
    bool canMove, cpt_only, canPromote;

    MoveList()
    : __begin(pMoves), __end(pMoves), pColor(1), cpt_only(false) {}

    inline void
    set(int t_cl, bool qs_only)
    {
        pColor = t_cl;
        cpt_only = qs_only;
        canMove = canPromote = false;
        __begin = __end = pMoves;
    }

    inline void Add(MoveType move) noexcept
    { *__end++ = move; }

    inline bool empty() const noexcept
    { return __begin == __end; }

    inline MoveType* begin() const noexcept
    { return __begin; }

    inline MoveType* end() const noexcept
    { return __end; }

    inline uint64_t size() const noexcept
    { return __end - __begin; }
};




// Returns and removes the lsb_idx
inline int next_idx(uint64_t &__n) {
    const int res = __builtin_ctzll(__n);
    __n &= __n - 1;
    return res;
}

// Prints all the info on the encoded-move
void decode_move(const MoveType encoded_move);






inline MoveType gen_base_move(const chessBoard& _cb, int ip);

inline void add_cap_moves(const int ip, uint64_t endSquares, const MoveType base,
    const chessBoard& _cb, MoveList& myMoves);

inline void add_quiet_moves(uint64_t endSquares, const MoveType base,
    const chessBoard& _cb, MoveList& myMoves);

void add_move_to_list(const int ip, uint64_t endSquares,
    const chessBoard& _cb, MoveList& myMoves);





void add_quiet_pawn_moves(uint64_t endSquares, const int shift,
    const chessBoard& _cb, MoveList& myMoves);








// Returns all squares attacked by all pawns from side to move
uint64_t pawn_atk_sq(const chessBoard& _cb, const int enemy);

// Returns all squares attacked by bishop on index __pos
uint64_t bishop_atk_sq(const int __pos, const uint64_t _Ap);

// Returns all squares attacked by knight on index __pos
uint64_t knight_atk_sq(const int __pos, const uint64_t _Ap);

// Returns all squares attacked by rook on index __pos
uint64_t rook_atk_sq(const int __pos, const uint64_t _Ap);

// Returns all squares attacked by queen on index __pos
uint64_t queen_atk_sq(const int __pos, const uint64_t _Ap);







/**
 * @brief Checks if player to move is in check
 * 
 * @param _cb board position
 * @return true 
 * @return false 
 */
bool Incheck(const chessBoard& _cb);

/**
 * @brief Returns string readable move from encoded-move.
 * Invalid move leads to undefined behaviour.
 * 
 * @param move encoded-move
 * @param _cb board position
 * @return string 
 */
string print(MoveType move, chessBoard& _cb);

/**
 * @brief Prints all encoded-moves in list to human-readable strings
 * 
 * @param _cb board position
 * @param myMoves Movelist for board positions.
 */
void print(const MoveList& myMoves, chessBoard& _cb);




#endif


