

#include "evaluation.h"

Evaluation ev;


// Search

// Late Move Pruning
// https://www.youtube.com/watch?v=R0L3AuJUkk0&ab_channel=TNGTechnologyConsultingGmbH


// -------------------------


// https://www.chessprogramming.org/Evaluation_of_Pieces

// Game-phase
// Material Weight
// Piece Mobility
// Piece-Square Table


// King-Safety (king and queen-side)
// Tempo-Bonus
// adjustements of piece value based on the number of own pawns
// Bonus of bishop-pair, penalty for [knight and rook] pairs
// Low material correction

  /********************************************************************
  *  Low material correction - guarding against an illusory material  *
  *  advantage. Full blown program should have more such rules,  but  *
  *  the current set ought to be useful enough. Please note that our  *
  *  code  assumes different material values for bishop and  knight.  *
  *                                                                   *
  *  - a single minor piece cannot win                                *
  *  - two knights cannot checkmate bare king                         *
  *  - bare rook vs minor piece is drawish                            *
  *  - rook and minor vs rook is drawish                              *
  ********************************************************************/


// special pawn_evaluation (after no major piece is left)

#ifndef MATERIAL

void
Evaluation::config_weights()
{
    float offset = (float)position_weight / 8000;
    // material_weight = abs_mat_weight + (offset / 4);
    positional_weight = abs_pos_weight + 1.5f * offset;
    mobility_weight = abs_mobility_weight - (1.5f * offset);
    pawn_struct_weight = abs_pawnstr_weight + (1 - (offset) / 4);
}

void
Evaluation::set_material_strength()
{
    pawnVal = 100; bishop_val = 330; knight_val = 300;
    rook_val = 500; queen_val = 900;

    /***
     * game_phase = 2 * (number of pieces (W + B)) + (pawns (W + B))
     * game_phase helps us to determine the state of the game
     * If game_phase > 32, we are in early middle game          (Values subjected to alter, depends on testing)
     * 16 <= game_phase <= 32 denotes the game is in middle_game
     * game_phase < 16 means we are in the end_game.

        * EARLY MIDDLE GAME:
        * Minor piece is stronger than 3 pawns
        * Knights can potentially strong as bishop, Rooks most likely to be inferior
        * Development takes first priority
        * Rooks may not contribute to the position this early on

        * MIDDLE GAME:
        * Bishops may be slightly stronger than knights, (open files should be taken into account)
        * Pass pawns contains huge potentials
        * Attacking Strength and King safety takes priority
        * Extra Points for Bishop Pair

        * END GAME:
        * Passed pawns are strong depending the number of pieces (should award extra points if its ahead of king to promotion sq.)
        * Rooks generally dominates
        * King Activity takes priority
        * Three pawns can be stronger than a pieces (should consider if pieces on board gets too low)
    ***/
    game_phase = 2 * pieceCount + (w_pawns + b_pawns);
    if (game_phase > 2 * phase_counter)
    {
        pawnVal -= 25;
        bishop_val -= 20;
        rook_val -= 25;
    }
    else if (game_phase > phase_counter)
    {
        pawnVal -= 15;
    }
    else if (game_phase < phase_counter)
    {
        pawnVal += 10;
        rook_val += 25;
    }
}

void
Evaluation::MaterialCount(const ChessBoard& _cb)
{
    w_pawns   = popcount(_cb.piece(WHITE, PAWN  ));
    w_bishops = popcount(_cb.piece(WHITE, BISHOP));
    w_knights = popcount(_cb.piece(WHITE, KNIGHT));
    w_rooks   = popcount(_cb.piece(WHITE, ROOK  ));
    w_queens  = popcount(_cb.piece(WHITE, QUEEN ));

    b_pawns   = popcount(_cb.piece(BLACK, PAWN  ));
    b_bishops = popcount(_cb.piece(BLACK, BISHOP));
    b_knights = popcount(_cb.piece(BLACK, KNIGHT));
    b_rooks   = popcount(_cb.piece(BLACK, ROOK  ));
    b_queens  = popcount(_cb.piece(BLACK, QUEEN ));

    w_pieces = w_bishops + w_knights + w_rooks + w_queens;
    b_pieces = b_bishops + b_knights + b_rooks + b_queens;
    pieceCount = w_pieces + b_pieces;

    position_weight =
        100 * (w_pawns   + b_pawns  ) + 320 * (w_bishops + b_bishops)
      + 300 * (w_knights + b_knights) + 500 * (w_rooks   + b_rooks  )
      + 925 * (w_queens  + b_queens );
}

int
Evaluation::material_strength()
{
    int wqn_val = queen_val + 8 * w_pawns;
    int bqn_val = queen_val + 8 * b_pawns;
    return   pawnVal * (  w_pawns -   b_pawns) + bishop_val * (w_bishops - b_bishops)
        + knight_val * (w_knights - b_knights) +   rook_val * (  w_rooks -   b_rooks)
        + ((wqn_val * w_queens) - (bqn_val * b_queens));
}

#endif

#ifndef SQUARE_TABLE

int
Evaluation::piece_strength(const ChessBoard& _cb) const
{
    const auto str_score = [](uint64_t piece, int *str_table)
    {
        int score = 0;
        while (piece != 0)
            score += str_table[next_idx(piece)];
        return score;
    };


    int pawn_white = str_score(_cb.piece(WHITE, PAWN), wpBoard);
    int pawn_black = str_score(_cb.piece(BLACK, PAWN), bpBoard);

    int bishop_white = str_score(_cb.piece(WHITE, BISHOP), wBoard);
    int bishop_black = str_score(_cb.piece(BLACK, BISHOP), bBoard);

    int knight_white = str_score(_cb.piece(WHITE, KNIGHT), NBoard);
    int knight_black = str_score(_cb.piece(BLACK, KNIGHT), NBoard);

    int rook_white = str_score(_cb.piece(WHITE, ROOK), wRBoard);
    int rook_black = str_score(_cb.piece(BLACK, ROOK), bRBoard);

    int score = (pawn_white - pawn_black)
              + (bishop_white - bishop_black)
              + 2 * (knight_white - knight_black)
              + (rook_white - rook_black);

    return score;
}

int
Evaluation::king_strength(const ChessBoard& _cb) const
{
    Square w_king = idx_no(_cb.piece(WHITE, KING));
    Square b_king = idx_no(_cb.piece(BLACK, KING));

    if (game_phase >= phase_counter)
        return WkMBoard[w_king] - BkMBoard[b_king];
    return WkEBoard[w_king] - BkEBoard[b_king];
}

#endif

#ifndef MOBILITY

int
Evaluation::piece_mobility(const ChessBoard &_cb) const
{
    const auto mobility_calc = [] (const auto &__f, uint64_t piece, uint64_t _Ap) {
        uint64_t area = 0;
        while (piece != 0)
            area |= __f(next_idx(piece), _Ap);

        return popcount(area);
    };

    uint64_t _Ap = _cb.piece(WHITE, PAWN) ^ _cb.piece(BLACK, PAWN);

    int bishop_white = mobility_calc(bishop_atk_sq, _cb.piece(WHITE, BISHOP), _Ap);
    int bishop_black = mobility_calc(bishop_atk_sq, _cb.piece(BLACK, BISHOP), _Ap);

    int knight_white = mobility_calc(knight_atk_sq, _cb.piece(WHITE, KNIGHT), _Ap);
    int knight_black = mobility_calc(knight_atk_sq, _cb.piece(BLACK, KNIGHT), _Ap);

    int rook_white = mobility_calc(rook_atk_sq, _cb.piece(WHITE, ROOK), _Ap);
    int rook_black = mobility_calc(rook_atk_sq, _cb.piece(BLACK, ROOK), _Ap);

    int score = (bishop_white - bishop_black)
              + 2 * (knight_white - knight_black)
              + (rook_white - rook_black);
    return score;
}

#endif

#ifndef PAWN_STRUCTURE

int PawnStructure(const ChessBoard& _cb, Color color)
{
    const auto dist_to_king = [] (int x, int y, int kpos)
    {
        int kx = kpos & 7;
        int ky = (kpos - kx) >> 3;
        return std::min(abs(x - kx), abs(y - ky));
    };

    int score = 0;
    uint64_t     pawns = _cb.piece( color, PAWN);
    uint64_t emy_pawns = _cb.piece(~color, PAWN);
    uint64_t    column = FileA;

    // Punish Double Pawns on same column
    for (int i = 0; i < 7; i++)
    {
        int p = popcount(column & pawns);
        score -= 32 * p * p;
        column <<= 1;
    }


    while (pawns != 0)
    {
        int idx = next_idx(pawns);
        if ((plt::PassedPawnMasks[color][idx] & emy_pawns) != 0)
            continue;

        int x = idx & 7;
        int y = (idx - x) >> 3;
        int d = (7 * (color ^ 1)) + y * (2 * color - 1);

        // Reward for having passed pawn
        score += 3 * d * d;

        int  kpos = idx_no(_cb.piece( color, KING));
        int ekpos = idx_no(_cb.piece(~color, KING));

        // Add score for king close to passed pawn and
        // Reduce score if enemy king is close to pawn

        int dist = dist_to_king(x, y, kpos) - dist_to_king(x, y, ekpos);
        score += (dist * dist * dist) / 2;
    }

    return score;
}

#endif

#ifndef ATTACK_STRENGTH

int
Evaluation::White_attk_Strength(const ChessBoard& _cb)
{
    uint64_t Apiece = _cb.All();
    int ekpos = idx_no( _cb.piece(BLACK, KING) );
    uint64_t attk_sq = plt::KingMasks[ekpos]
                      | (ekpos > 7 ? plt::KingMasks[ekpos - 8] : 0);

    const auto add_attackers = [attk_sq, Apiece] (const auto &__f, uint64_t piece) {
        int res = 0;
        while (piece) {
            int idx = next_idx(piece);
            uint64_t area = __f(idx, Apiece);
            if (area & attk_sq) res++;
        }
        return res;
    };

    int attackers = 0;

    attackers += add_attackers(bishop_atk_sq, _cb.piece(WHITE, BISHOP));
    attackers += add_attackers(knight_atk_sq, _cb.piece(WHITE, KNIGHT));
    attackers += add_attackers(rook_atk_sq  , _cb.piece(WHITE, ROOK)  );
    attackers += add_attackers(queen_atk_sq , _cb.piece(WHITE, QUEEN) );

    attackers = std::min(attackers, 4);
    return (1 << (attackers * 3));
}

int
Evaluation::Black_attk_Strength(const ChessBoard& _cb)
{
    uint64_t Apiece = _cb.All();
    int ekpos = idx_no( _cb.piece(WHITE, KING) );
    uint64_t attk_sq = plt::KingMasks[ekpos]
                      | (ekpos < 56 ? plt::KingMasks[ekpos + 8] : 0);

    const auto add_attackers = [attk_sq, Apiece] (const auto &__f, uint64_t piece)
    {
        int res = 0;
        while (piece)
        {
            int idx = next_idx(piece);
            uint64_t area = __f(idx, Apiece);
            if (area & attk_sq) res++;
        }
        return res;
    };

    int attackers = 0;

    attackers += add_attackers(bishop_atk_sq, _cb.piece(BLACK, BISHOP));
    attackers += add_attackers(knight_atk_sq, _cb.piece(BLACK, KNIGHT));
    attackers += add_attackers(rook_atk_sq  , _cb.piece(BLACK, ROOK)  );
    attackers += add_attackers(queen_atk_sq , _cb.piece(BLACK, QUEEN) );

    attackers = std::min(attackers, 4);
    return (1 << (attackers * 3));
}

int
Evaluation::White_King_Safety(const ChessBoard& _cb)
{
    uint64_t res;
    int kpos = idx_no( _cb.piece(WHITE, KING) );
    int kx = kpos & 7, ky = (kpos - kx) >> 3;
    int score = 0, open_files = 0, defenders[2];

    res = plt::UpMasks[kpos];
    if (!(res & _cb.piece(WHITE, PAWN))) open_files++;
    if (kx) {
        res = plt::UpMasks[kpos - 1];
        if (!(res & _cb.piece(WHITE, PAWN))) open_files++;
    }
    if (kx != 7) {
        res = plt::UpMasks[kpos + 1];
        if (!(res & _cb.piece(WHITE, PAWN))) open_files++;
    }
    score -= 24 * (open_files * open_files);
    
    res = plt::KingMasks[kpos];
    if (ky != 7) res |= plt::KingMasks[kpos + 8];
    defenders[0] = popcount((res & (_cb.piece(WHITE, BISHOP) ^ _cb.piece(WHITE, KNIGHT) ^ _cb.piece(WHITE, ROOK) ^ _cb.piece(WHITE, QUEEN))));
    defenders[1] = popcount((res & _cb.piece(WHITE, PAWN)));

    score += (1 << (defenders[0] * 3)) + 12 * (defenders[1] * defenders[1]);
    return score;
}

int
Evaluation::Black_King_Safety(const ChessBoard& _cb)
{
    uint64_t res;
    int kpos = idx_no( _cb.piece(BLACK, KING) );
    int kx = kpos & 7, ky = (kpos - kx) >> 3;
    int score = 0, open_files = 0, defenders[2];

    res = plt::DownMasks[kpos];
    if (!(res & _cb.piece(BLACK, PAWN))) open_files++;
    if (kx)
    {
        res = plt::DownMasks[kpos - 1];
        if (!(res & _cb.piece(BLACK, PAWN))) open_files++;
    }
    if (kx != 7)
    {
        res = plt::DownMasks[kpos + 1];
        if (!(res & _cb.piece(BLACK, PAWN))) open_files++;
    }
    score -= 24 * (open_files * open_files);
    
    res = plt::KingMasks[kpos];
    if (ky) res |= plt::KingMasks[kpos - 8];
    defenders[0] = popcount((res & (_cb.piece(BLACK, KNIGHT) ^ _cb.piece(BLACK, BISHOP) ^ _cb.piece(BLACK, ROOK) ^ _cb.piece(BLACK, QUEEN))));
    defenders[1] = popcount((res & _cb.piece(BLACK, PAWN)));

    score += (1 << (defenders[0] * 3)) + 12 * (defenders[1] * defenders[1]);
    return score;
}

int
Evaluation::attack_strength(const ChessBoard& _cb)
{
    if (game_phase < phase_counter + 2) return 0;

    int white_safety = White_King_Safety(_cb), black_safety = Black_King_Safety(_cb);
    int x =  White_attk_Strength(_cb), y = Black_attk_Strength(_cb);
    
    int white_attack = std::max(x - black_safety, 0);
    int black_attack = std::max(y - white_safety, 0);

    return white_attack - black_attack;
}

#endif

bool Evaluation::is_hypothetical_draw()
{
    if (w_pieces >= 3 || b_pieces >= 3) return false;
    if (w_pawns + b_pawns) return false;

    if (w_pieces == 1 && b_pieces == 1)
    {
        if ((w_bishops + w_knights) == 1 && b_rooks == 1) return true;
        if (w_rooks == 1 && (b_bishops + b_knights) == 1) return true;
    }

    if (w_pieces == 1 && !b_pieces) {
        if (w_bishops == 1 || w_knights == 1) return true;
    }
    if (b_pieces == 1 && !w_pieces) {
        if (b_bishops == 1 || b_knights == 1) return true;
    }
    if (w_pieces + b_pieces == 2 && w_pieces == b_pieces)
    {
        if (w_queens + b_queens == 2) return true;
        if (w_rooks  + b_rooks  == 2) return true;
        if (w_bishops + b_bishops == 2) return true;
        if (w_knights + b_knights == 2) return true;
    }
    if (w_pieces + b_pieces == 2) {
        if (w_knights == 2 || b_knights == 2) return true;
    }

    return false;
}

void Evaluation::set_parameter (int tmp1, int tmp2, int tmp3, int tmp4, int tmp5)
{
    material_weight = (float)tmp1 / 10;
    positional_weight = (float)tmp2 / 10;
    attk_str_weight = (float)tmp3 / 10;
    mobility_weight = (float)tmp4 / 10;
    pawn_struct_weight = (float)tmp5 / 10;
}

int Evaluation::Evaluate (const ChessBoard& _cb)
{
    MaterialCount(_cb);

    if (!position_weight) return 0;

    set_material_strength();
    float material_diff = static_cast<float>(material_strength());

    if (game_phase <= 8 && is_hypothetical_draw()) return 0;
    // config_weights();

    float pos_Str = static_cast<float>(piece_strength(_cb) + king_strength(_cb));
    float mobility = static_cast<float>(piece_mobility(_cb));
    float pawn_structure = static_cast<float>(PawnStructure(_cb, Color::WHITE) - PawnStructure(_cb, Color::BLACK));
    float attack_value = static_cast<float>(attack_strength(_cb));

    float eval = (material_weight * material_diff) + (positional_weight * pos_Str) 
                + (attk_str_weight * attack_value) + (mobility_weight * mobility) 
                + (pawn_struct_weight * pawn_structure);


    // if (std::abs(eval) > 3200) {
    //     cout << "Fen : " << _cb.fen() << endl;
    //     cout << "Attack Value : " << (attk_str_weight * attack_value) << endl;
    // }

    return static_cast<int>(eval) * (2 * _cb.color - 1);
}

