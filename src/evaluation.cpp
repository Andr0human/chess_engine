

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
    game_phase = 2 * pieceCount + (wPawns + bPawns);
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
    wPawns   = __ppcnt(PAWN(WHITE));     bPawns = __ppcnt(PAWN(BLACK));
    wBishops = __ppcnt(BISHOP(WHITE)); bBishops = __ppcnt(BISHOP(BLACK));
    wKnights = __ppcnt(KNIGHT(WHITE)); bKnights = __ppcnt(KNIGHT(BLACK));
    wRooks   = __ppcnt(ROOK(WHITE));     bRooks = __ppcnt(ROOK(BLACK));
    wQueens  = __ppcnt(QUEEN(WHITE));   bQueens = __ppcnt(QUEEN(BLACK));
    wPieces = wBishops + wKnights + wRooks + wQueens;
    bPieces = bBishops + bKnights + bRooks + bQueens;
    pieceCount = wPieces + bPieces;
    position_weight = 100 * (wPawns + bPawns) + 320 * (wBishops + bBishops)
                    + 300 * (wKnights + bKnights) + 500 * (wRooks + bRooks)
                    + 925 * (wQueens + bQueens);
}

int
Evaluation::material_strength()
{
    const int wqn_val = queen_val + 8 * wPawns;
    const int bqn_val = queen_val + 8 * bPawns;
    return   pawnVal * (  wPawns -   bPawns) + bishop_val * (wBishops - bBishops)
        + knight_val * (wKnights - bKnights) +   rook_val * (  wRooks -   bRooks)
        + ((wqn_val * wQueens) - (bqn_val * bQueens));
}

#endif

#ifndef SQUARE_TABLE

int
Evaluation::piece_strength(const ChessBoard &_cb) const
{
    const auto str_score = [](uint64_t piece, const int *str_table)
    {
        int score = 0;
        while (piece != 0)
            score += str_table[next_idx(piece)];
        return score;
    };


    const int pawn_white = str_score(PAWN(WHITE), wpBoard);
    const int pawn_black = str_score(PAWN(BLACK), bpBoard);

    const int bishop_white = str_score(BISHOP(WHITE), wBoard);
    const int bishop_black = str_score(BISHOP(BLACK), bBoard);

    const int knight_white = str_score(KNIGHT(WHITE), NBoard);
    const int knight_black = str_score(KNIGHT(BLACK), NBoard);

    const int rook_white = str_score(ROOK(WHITE), wRBoard);
    const int rook_black = str_score(ROOK(BLACK), bRBoard);

    const int score = (pawn_white - pawn_black)
                    + (bishop_white - bishop_black)
                    + 2 * (knight_white - knight_black)
                    + (rook_white - rook_black);

    return score;
}

int
Evaluation::king_strength(const ChessBoard& _cb) const
{
    if (game_phase >= phase_counter)
        return WkMBoard[idx_no(KING(WHITE))] - BkMBoard[idx_no(KING(BLACK))];
    return WkEBoard[idx_no(KING(WHITE))] - BkEBoard[idx_no(KING(BLACK))];
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

        return __ppcnt(area);
    };

    const uint64_t _Ap = PAWN(WHITE) ^ PAWN(BLACK);

    const int bishop_white = mobility_calc(bishop_atk_sq, BISHOP(WHITE), _Ap);
    const int bishop_black = mobility_calc(bishop_atk_sq, BISHOP(BLACK), _Ap);

    const int knight_white = mobility_calc(knight_atk_sq, KNIGHT(WHITE), _Ap);
    const int knight_black = mobility_calc(knight_atk_sq, KNIGHT(BLACK), _Ap);

    const int rook_white = mobility_calc(rook_atk_sq, ROOK(WHITE), _Ap);
    const int rook_black = mobility_calc(rook_atk_sq, ROOK(BLACK), _Ap);

    const int score = (bishop_white - bishop_black)
                    + 2 * (knight_white - knight_black)
                    + (rook_white - rook_black);
    return score;
}

#endif

#ifndef PAWN_STRUCTURE

int
Evaluation::WhitePawns_Structure(const ChessBoard& _cb)
{
    uint64_t pawns = PAWN(WHITE), val, res;
    int score = 0, corner_points = 25, central_points = 15, pass_points = 10;
    if (game_phase < 20)
    {
        score = 60;
        pass_points = 30;
        corner_points = 50;
        central_points = 30;
    }

    while (pawns)
    {
        val = pawns ^ (pawns & (pawns - 1));
        int idx = idx_no(val), x = idx & 7, y = (idx - x) >> 3;
        res = plt::UpMasks[idx] & PAWN(BLACK);
        if (!res) score += pass_points + (4 * y);
        if (x == 0)
        {
            res = plt::UpMasks[idx + 1] & PAWN(BLACK);
            if (!res) score += corner_points + (4 * y);
        }
        else if (x == 7)
        {
            res = plt::UpMasks[idx - 1] & PAWN(BLACK);
            if (!res) score += corner_points + (4 * y);
        }
        else if (x > 0 && x < 7)
        {
            res = (plt::UpMasks[idx + 1] ^ plt::UpMasks[idx - 1]) & PAWN(BLACK);
            if (!res) score += central_points + (4 * y);
        }
        pawns &= pawns - 1;
    }

    uint64_t column = 72340172838076673ULL;
    for (int i = 0; i < 7; i++)
    {
        int cnt = __ppcnt((column & PAWN(WHITE)));
        if (cnt == 2) score -= 40;
        if (cnt >= 3) score -= 120;
        column <<= 1;
    }
    return score;
}

int
Evaluation::BlackPawns_Structure(const ChessBoard& _cb)
{
    uint64_t pawns = PAWN(BLACK), val, res;
    int score = 0, corner_points = 25, central_points = 15, pass_points = 10;
    if (game_phase < 20)
    {
        score = 60;
        pass_points = 30;
        corner_points = 50;
        central_points = 30;
    }
    while (pawns)
    {
        val = pawns ^ (pawns & (pawns - 1));
        int idx = idx_no(val), x = idx & 7, y = (idx - x) >> 3;
        res = plt::DownMasks[idx] & PAWN(WHITE);
        if (!res) score += pass_points + (4 * (7 - y));
        if (x == 0)
        {
            res = plt::DownMasks[idx + 1] & PAWN(WHITE);
            if (!res) score += corner_points + (4 * (7 - y));
        }
        else if (x == 7)
        {
            res = plt::DownMasks[idx - 1] & PAWN(WHITE);
            if (!res) score += corner_points + (4 * (7 - y));
        }
        else if (x > 0 && x < 7)
        {
            res = (plt::DownMasks[idx + 1] ^ plt::UpMasks[idx - 1]) & PAWN(WHITE);
            if (!res) score += central_points + (4 * (7 - y));
        }
        pawns &= pawns - 1;
    }
    uint64_t column = 72340172838076673ULL;
    for (int i = 0; i < 7; i++)
    {
        int cnt = __ppcnt((column & PAWN(BLACK)));
        if (cnt == 2) score -= 40;
        if (cnt >= 3) score -= 120;
        column <<= 1;
    }
    return score;
}

#endif

#ifndef ATTACK_STRENGTH

int
Evaluation::White_attk_Strength(const ChessBoard& _cb)
{
    const uint64_t Apiece = ALL_BOTH;
    const int kpos = idx_no(KING(BLACK));
    const uint64_t attk_sq = plt::KingMasks[kpos]
                      | (kpos > 7 ? plt::KingMasks[kpos - 8] : 0);

    const auto add_attackers = [attk_sq, Apiece] (const auto &__f, uint64_t piece) {
        int res = 0;
        while (piece) {
            const int idx = next_idx(piece);
            const uint64_t area = __f(idx, Apiece);
            if (area & attk_sq) res++;
        }
        return res;
    };

    int attackers = 0;

    attackers += add_attackers(bishop_atk_sq, BISHOP(WHITE));
    attackers += add_attackers(knight_atk_sq, KNIGHT(WHITE));
    attackers += add_attackers(rook_atk_sq  , ROOK(WHITE)  );
    attackers += add_attackers(queen_atk_sq , QUEEN(WHITE) );

    attackers = std::min(attackers, 4);
    return (1 << (attackers * 3));
}

int
Evaluation::Black_attk_Strength(const ChessBoard& _cb)
{
    const uint64_t Apiece = ALL_BOTH;
    const int kpos = idx_no(KING(WHITE));
    const uint64_t attk_sq = plt::KingMasks[kpos]
                      | (kpos < 56 ? plt::KingMasks[kpos + 8] : 0);

    const auto add_attackers = [attk_sq, Apiece] (const auto &__f, uint64_t piece)
    {
        int res = 0;
        while (piece)
        {
            const int idx = next_idx(piece);
            const uint64_t area = __f(idx, Apiece);
            if (area & attk_sq) res++;
        }
        return res;
    };

    int attackers = 0;

    attackers += add_attackers(bishop_atk_sq, BISHOP(BLACK));
    attackers += add_attackers(knight_atk_sq, KNIGHT(BLACK));
    attackers += add_attackers(rook_atk_sq  , ROOK(BLACK)  );
    attackers += add_attackers(queen_atk_sq , QUEEN(BLACK) );

    attackers = std::min(attackers, 4);
    return (1 << (attackers * 3));
}

int
Evaluation::White_King_Safety(const ChessBoard& _cb)
{
    uint64_t res;
    int kpos = idx_no(KING(WHITE)), kx = kpos & 7, ky = (kpos - kx) >> 3;
    int score = 0, open_files = 0, defenders[2];

    res = plt::UpMasks[kpos];
    if (!(res & PAWN(WHITE))) open_files++;
    if (kx) {
        res = plt::UpMasks[kpos - 1];
        if (!(res & PAWN(WHITE))) open_files++;
    }
    if (kx != 7) {
        res = plt::UpMasks[kpos + 1];
        if (!(res & PAWN(WHITE))) open_files++;
    }
    score -= 24 * (open_files * open_files);
    
    res = plt::KingMasks[kpos];
    if (ky != 7) res |= plt::KingMasks[kpos + 8];
    defenders[0] = __ppcnt((res & (BISHOP(WHITE) ^ KNIGHT(WHITE) ^ ROOK(WHITE) ^ QUEEN(WHITE))));
    defenders[1] = __ppcnt((res & PAWN(WHITE)));

    score += (1 << (defenders[0] * 3)) + 12 * (defenders[1] * defenders[1]);
    return score;
}

int
Evaluation::Black_King_Safety(const ChessBoard& _cb)
{
    uint64_t res;
    int kpos = idx_no(KING(BLACK)), kx = kpos & 7, ky = (kpos - kx) >> 3;
    int score = 0, open_files = 0, defenders[2];

    res = plt::DownMasks[kpos];
    if (!(res & PAWN(BLACK))) open_files++;
    if (kx)
    {
        res = plt::DownMasks[kpos - 1];
        if (!(res & PAWN(BLACK))) open_files++;
    }
    if (kx != 7)
    {
        res = plt::DownMasks[kpos + 1];
        if (!(res & PAWN(BLACK))) open_files++;
    }
    score -= 24 * (open_files * open_files);
    
    res = plt::KingMasks[kpos];
    if (ky) res |= plt::KingMasks[kpos - 8];
    defenders[0] = __ppcnt((res & (KNIGHT(BLACK) ^ BISHOP(BLACK) ^ ROOK(BLACK) ^ QUEEN(BLACK))));
    defenders[1] = __ppcnt((res & PAWN(BLACK)));

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
    if (wPieces >= 3 || bPieces >= 3) return false;
    if (wPawns + bPawns) return false;

    if (wPieces == 1 && bPieces == 1)
    {
        if ((wBishops + wKnights) == 1 && bRooks == 1) return true;
        if (wRooks == 1 && (bBishops + bKnights) == 1) return true;
    }

    if (wPieces == 1 && !bPieces) {
        if (wBishops == 1 || wKnights == 1) return true;
    }
    if (bPieces == 1 && !wPieces) {
        if (bBishops == 1 || bKnights == 1) return true;
    }
    if (wPieces + bPieces == 2 && wPieces == bPieces)
    {
        if (wQueens + bQueens == 2) return true;
        if (wRooks  + bRooks  == 2) return true;
        if (wBishops + bBishops == 2) return true;
        if (wKnights + bKnights == 2) return true;
    }
    if (wPieces + bPieces == 2) {
        if (wKnights == 2 || bKnights == 2) return true;
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
    float pawn_structure = static_cast<float>(WhitePawns_Structure(_cb) - BlackPawns_Structure(_cb));
    float attack_value = static_cast<float>(attack_strength(_cb));

    float eval = (material_weight * material_diff) + (positional_weight * pos_Str) 
                + (attk_str_weight * attack_value) + (mobility_weight * mobility) 
                + (pawn_struct_weight * pawn_structure);


    // if (std::abs(eval) > 3200) {
    //     cout << "Fen : " << _cb.fen() << endl;
    //     cout << "Attack Value : " << (attk_str_weight * attack_value) << endl;
    //     // _cb.show();
    // }

    return static_cast<int>(eval) * (2 * _cb.color - 1);
}

