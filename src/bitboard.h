
#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"
#include "base_utils.h"
#include "tt.h"


extern uint64_t tmp_total_counter;
extern uint64_t tmp_this_counter;


class UndoInfo
{
    public:
    // Last Move
    Move move;

    // Last castle and en-passant states.
    int csep;

    // Last Position Hash
    Key hash;

    //! TODO Remove this.
    // Last HalfMove
    int halfmove;

    UndoInfo() : move(0), csep(0), hash(0), halfmove(0) {}

    UndoInfo(Move m, int c, Key h, int hm)
    : move(m), csep(c), hash(h), halfmove(hm) {}
};




class ChessBoard
{
    private:
    static const int undoInfoSize = 300;

    UndoInfo undo_info[undoInfoSize];

    int moveNum = 0;

    // Stores the piece at each index
    Piece board[64];

    // Stores bitboard location of a piece
    Bitboard piece_bb[16];

    // MakeMove-Subparts

    bool
    is_en_passant(Square fp, Square ep) const noexcept
    { return fp == ep; }

    bool
    is_double_pawn_push(Square ip, Square fp) const noexcept
    { return std::abs(ip - fp) == 16; }

    bool
    is_pawn_promotion(Square fp) const noexcept
    { return (1ULL << fp) & 0xFF000000000000FF; }

    bool
    is_castling(Square ip, Square fp) const noexcept
    { return (ip - fp == 2) | (fp - ip == 2);}

    void
    make_move_castle_check(PieceType p, Square sq) noexcept;

    void
    make_move_enpassant(Square ip, Square fp) noexcept;

    void
    make_move_double_pawn_push(Square ip, Square fp) noexcept;

    void
    make_move_pawn_promotion(Move move) noexcept;

    void
    make_move_castling(Square ip, Square fp, int call_from_makemove) noexcept;

    void
    update_csep(int old_csep, int new_csep) noexcept;

    public:

    // White -> 1, Black -> 0
    Color color;

    int csep, halfmove, fullmove;
    Key Hash_Value;

    //  Number of opponent pieces threatening the king for the side to move
    int KA;

    // Bitboard representing the squares that the pieces of the current
    // side can legally move to when the king is under check
    uint64_t legal_squares_mask;

    // Bitboard representing squares under enemy attack
    uint64_t enemy_attacked_squares;

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


    ChessBoard();

    ChessBoard(const std::string& fen);

    void
    set_position_with_fen(const string& fen) noexcept;

    string
    visual_board() const noexcept;

    void
    MakeMove(Move move, bool in_search = true) noexcept;

    void
    UnmakeMove() noexcept;

    void
    UndoInfoPush(PieceType it, PieceType ft, Move move, bool in_search);

    Move
    UndoInfoPop();

    const string
    fen() const;

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

    inline bool
    enemy_attacked_sq_generated() const noexcept
    { return /* piece_bb[8] != 0; */ enemy_attacked_squares != 0; }

    inline bool
    attackers_found() const noexcept
    { return KA != -1;  }

    inline void
    remove_movegen_extra_data() noexcept
    {
        KA = -1;
        legal_squares_mask = 0;
        enemy_attacked_squares = 0;
    }

    inline bool
    king_in_check() const noexcept
    { return KA > 0; }

    inline Piece
    piece_on_square(Square sq) const noexcept
    { return board[sq]; }

    inline Bitboard
    piece(Color c, PieceType pt) const noexcept
    { return piece_bb[make_piece(c, pt)]; }

    inline Bitboard
    All() const noexcept
    {
        Piece white_all = make_piece(WHITE, ALL);
        Piece black_all = make_piece(BLACK, ALL);
        return piece_bb[white_all] | piece_bb[black_all];
    }

    void
    dump(std::ostream& writer = std::cout);
};


/*
                    ------   MOVE   ------

            str.    clr  ppt  cpt  pt  fp     ip
            00000   1    00   000  000 000000 000000
            25      20   19   17   14  11     5

*/

#endif

