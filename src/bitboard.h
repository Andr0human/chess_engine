
#ifndef BITBOARD_H
#define BITBOARD_H

#include "tt.h"
#include <iostream>
#include <cstring>          // For memset
#include <vector>


using std::cout;
using std::endl;
using std::string;
using std::vector;

using MoveType = int;

extern uint64_t tmp_total_counter;
extern uint64_t tmp_this_counter;


#ifndef BIT_MANIPULATION

inline int
__ppcnt(const uint64_t N) 
{ return __builtin_popcountll(N); }

inline int
idx_no(const uint64_t N)
{ return __builtin_popcountll(N - 1); }

inline int
lSb_idx(const uint64_t N)
{ return __builtin_ctzll(N | (1ULL << 63)); }

inline int
mSb_idx(const uint64_t N)
{ return __builtin_clzll(N | 1) ^ 63; }

inline uint64_t
lSb(const uint64_t N)
{ return N ^ (N & (N - 1)); }

inline uint64_t
mSb(const uint64_t N)
{ return (N!=0) ? (1ULL << (__builtin_clzll(N)^63)) : (0); }

inline uint64_t
l_shift(const uint64_t val, const int shift)
{ return val << shift; }

inline uint64_t
r_shift(const uint64_t val, const int shift)
{ return val >> shift; }

#endif

class chessBoard
{
    private:
    static const int pieceOfs = 470, ofs = 7;
    static const int max_moveNum = 60;

    MoveType aux_table_move[max_moveNum];
    int aux_table_csep[max_moveNum];
    uint64_t aux_table_hash[max_moveNum];
    int moveNum = 0;

    void
    fill_with_piece(std::string arr[], uint64_t val, char ch)
    const;

    // MakeMove-Subparts

    bool
    is_en_passant(int fp, int ep) const noexcept
    { return fp == ep; }

    bool
    is_double_pawn_push(int ip, int fp) const noexcept
    { return std::abs(ip - fp) == 16; }

    bool
    is_pawn_promotion(int fp) const noexcept
    { return (1ULL << fp) & 0xFF000000000000FF; }

    bool
    is_castling(int ip, int fp) const noexcept
    { return (ip - fp == 2) | (fp - ip == 2);}

    void
    make_move_castle_check(const int piece, const int sq);

    void
    make_move_enpassant(int ip, int fp);

    void
    make_move_double_pawn_push(int ip, int fp);

    void
    make_move_pawn_promotion(const MoveType move);

    void
    make_move_castling(int ip, int fp, bool call_from_makemove);

    void
    update_csep(int old_csep, int new_csep);

    public:
    int board[64]{0};

    // White -> 1, Black -> 0
    int color;

    int KA;
    int csep, halfmove, fullmove;
    uint64_t Hash_Value;

    /**
     * 1 -> Pawn
     * 2 -> Bishop
     * 3 -> Knight
     * 4 -> Rook
     * 5 -> Queen
     * 6 -> King
     * 7 -> All pieces
     * 
     * Black -> 0
     * White -> 8
     * Black King -> Black + King = 0 + 6 =  6
     * White Rook -> White + Rook = 8 + 4 = 12
     * 
     * Own -> color << 3
     * Enemy -> (color ^ 1) << 3
     * 
     * Own Knight -> Own + Knight = color << 3 + Knight
     * Emy Bishop -> Emy + Bishop = (color ^ 1) << 3 + Bishop
     * 
     * Pieces[0] and Pieces[8] are unused.
     * 
     **/
    uint64_t Pieces[16]{0};

    chessBoard();
    
    chessBoard(const std::string& fen);

    void
    set_position_with_fen(const string& fen) noexcept;

    void
    show() const noexcept;

    void
    MakeMove(const MoveType move);

    void
    UnmakeMove();

    void
    auxilary_table_update(const MoveType move);

    int
    auxilary_table_revert();

    const string
    fen() const;

    void
    current_line(MoveType moves[], int& __n) const noexcept
    {
        for (int i = 0; i < moveNum; i++)
            moves[i] = aux_table_move[i];
        __n = moveNum;
    }

    bool
    three_move_repetition() const noexcept;

    uint64_t
    generate_hashKey() const;

    void
    MakeNullMove();

    void
    UnmakeNullMove();

    void
    Reset();

    bool operator== (const chessBoard& other);

    bool operator!= (const chessBoard& other);

    inline bool enemy_attacked_sq_generated() const noexcept
    { return Pieces[8] != 0; }

    inline bool attackers_found() const noexcept
    { return KA != -1; }

    inline void remove_movegen_extra_data() noexcept
    { KA = -1, Pieces[8] = 0, Pieces[0] = 0; }

    inline bool king_in_check() const noexcept
    { return KA > 0; }
};


/*
                    ------   MOVE   ------

            str.    clr  ppt  cpt  pt  fp     ip
            00000   1    00   000  000 000000 000000
            25      20   19   17   14  11     5


    nuint64_t = 0
    captures = 1
    castle = 2
    en-passant = 3
    promotion = 4, 5, 6, 7


*/

#endif

