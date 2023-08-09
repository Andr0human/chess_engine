
#ifndef BITBOARD_H
#define BITBOARD_H


#include "base_utils.h"
#include "tt.h"


extern uint64_t tmp_total_counter;
extern uint64_t tmp_this_counter;


class ChessBoard
{
    private:
    static const int pieceOfs = 470, ofs = 7;
    static const int max_moveNum = 300;

    MoveType aux_table_move[max_moveNum];
    int aux_table_csep[max_moveNum];
    uint64_t aux_table_hash[max_moveNum];
    int aux_table_halfmove[max_moveNum];
    int moveNum = 0;


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
    make_move_castle_check(int piece, int sq) noexcept;

    void
    make_move_enpassant(int ip, int fp) noexcept;

    void
    make_move_double_pawn_push(int ip, int fp) noexcept;

    void
    make_move_pawn_promotion(const MoveType move) noexcept;

    void
    make_move_castling(int ip, int fp, bool call_from_makemove) noexcept;

    void
    update_csep(int old_csep, int new_csep) noexcept;

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

    ChessBoard();
    
    ChessBoard(const std::string& fen);

    void
    set_position_with_fen(const string& fen) noexcept;

    string
    visual_board() const noexcept;

    void
    MakeMove(const MoveType move) noexcept;

    void
    UnmakeMove() noexcept;

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

    bool
    fifty_move_rule_draw() const noexcept;

    void
    add_prev_board_positions(const vector<uint64_t>& prev_keys) noexcept;

    uint64_t
    generate_hashKey() const;

    void
    MakeNullMove();

    void
    UnmakeNullMove();

    void
    Reset();

    bool operator== (const ChessBoard& other);

    bool operator!= (const ChessBoard& other);

    inline bool enemy_attacked_sq_generated() const noexcept
    { return Pieces[8] != 0; }

    inline bool attackers_found() const noexcept
    { return KA != -1; }

    inline void remove_movegen_extra_data() noexcept
    { KA = -1, Pieces[8] = 0, Pieces[0] = 0; }

    inline bool king_in_check() const noexcept
    { return KA > 0; }

    inline bool side() const noexcept
    { return color; }

    inline bool side_emy() const noexcept
    { return color ^ 1; }

    inline int king_pos() const noexcept
    { return idx_no(Pieces[(color << 3) + 6]); }

    inline int king_pos_emy() const noexcept
    { return idx_no(Pieces[((color ^ 1) << 3) + 6]); }
};


/*
                    ------   MOVE   ------

            str.    clr  ppt  cpt  pt  fp     ip
            00000   1    00   000  000 000000 000000
            25      20   19   17   14  11     5

*/

#endif

