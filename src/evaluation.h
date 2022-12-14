

#ifndef EVALUATION_H
#define EVALUATION_H

#include "movegen.h"
#include "PieceSquareTable.h"


class Evaluation
{
    int wPawns, wKnights, wBishops, wRooks, wQueens, wPieces;
    int bPawns, bKnights, bBishops, bRooks, bQueens, bPieces;
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
    void MaterialCount(const chessBoard& _cb);
    int material_strength();

    // PIECE_SQUARE_TABLE_STRENGTH
    int piece_strength(const chessBoard &_cb) const;
    int king_strength(const chessBoard& _cb) const;

    // MOBILITY
    int piece_mobility(const chessBoard &_cb) const;

    int WhitePawns_Structure(const chessBoard& _cb);
    int BlackPawns_Structure(const chessBoard& _cb);

    // ATTACK_STRENGTH
    int White_attk_Strength(const chessBoard& _cb);
    int Black_attk_Strength(const chessBoard& _cb);
    int White_King_Safety(const chessBoard& _cb);
    int Black_King_Safety(const chessBoard& _cb);
    int attack_strength(const chessBoard& _cb);

    public:
    // Set the weights for evaluation
    void set_parameter(int tmp1, int tmp2, int tmp3, int tmp4, int tmp5);


    int Evaluate (const chessBoard& _cb);
};

extern Evaluation ev;

#endif

