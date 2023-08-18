

#ifndef EVALUATION_H
#define EVALUATION_H

#include "movegen.h"
#include "PieceSquareTable.h"


class Evaluation
{
    int w_pawns, w_knights, w_bishops, w_rooks, w_queens, w_pieces;
    int b_pawns, b_knights, b_bishops, b_rooks, b_queens, b_pieces;

    int pawnVal = 100, bishop_val = 330, knight_val = 300, rook_val = 500, queen_val = 900;
    int position_weight = 0;
    int pieceCount = 0, game_phase = 0, phase_counter = 16;

    float abs_mat_weight = 1.0f, abs_pos_weight = 0.8f, abs_attkstr_weight = 0.5f;
    float abs_mobility_weight = 2.6f, abs_pawnstr_weight = 0.4f;
    
    float material_weight = 1.0f, positional_weight = 1.8f, attk_str_weight = 0.5f;
    float mobility_weight = 1.2f, pawn_struct_weight = 0.4f;

    bool is_hypothetical_draw();

    void config_weights();
    void set_material_strength();
    void MaterialCount(const ChessBoard& _cb);
    int material_strength();

    // PIECE_SQUARE_TABLE_STRENGTH
    int piece_strength(const ChessBoard &_cb) const;
    int king_strength(const ChessBoard& _cb) const;

    // MOBILITY
    int piece_mobility(const ChessBoard &_cb) const;

    // ATTACK_STRENGTH
    int White_attk_Strength(const ChessBoard& _cb);
    int Black_attk_Strength(const ChessBoard& _cb);
    int White_King_Safety(const ChessBoard& _cb);
    int Black_King_Safety(const ChessBoard& _cb);
    int attack_strength(const ChessBoard& _cb);

    public:
    // Set the weights for evaluation
    void set_parameter(int tmp1, int tmp2, int tmp3, int tmp4, int tmp5);


    int Evaluate (const ChessBoard& _cb);
};

extern Evaluation ev;

#endif

