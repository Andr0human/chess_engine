
#include "movegen.h"


/**
 * @brief Add_pawn consideration
 * cap promotion still possible while pinned
 * 
 */


#ifndef MOVEGEN_UTILS

static inline MoveType
gen_move_priority(const chessBoard& __b, const int ip, const int fp)
{
    int res = 0;
    const int pt  =  __b.board[ip] & 7;
    const int cpt = -__b.board[fp] & 7;
    const int is_cpt = (bool)cpt;

    res += (cpt - pt + 16) * is_cpt;
    return res;
}

static inline MoveType
gen_pawn_move_priority(const chessBoard& __b, const int ip, const int fp)
{
    int res = 5;
    const int pt  = __b.board[ip] & 7;
    const int cpt = __b.board[fp] & 7;

    res += cpt ? (cpt - pt + 16) : 0;
    return res;
}

static inline MoveType
encode_move(const chessBoard &__b, const int ip, const int fp, const int pr)
{
    const int pt  = __b.board[ip] & 7;
    const int cpt = __b.board[fp] & 7;
    const int white_move = 1048576;
    
    int gen_move = __b.board[ip] > 8 ? white_move : 0;
    
    gen_move ^= (pr << 21) ^ (cpt << 15) ^ (pt << 12) ^ (fp << 6) ^ ip;
    return gen_move;
}

static bool
en_passant_recheck(int ip, const chessBoard& _cb)
{
    const int own = _cb.color << 3;
    const int emy = (_cb.color ^ 1) << 3;

    const int kpos = idx_no(KING(own));
    const int eps = _cb.csep & 127;
    const uint64_t erq = QUEEN(emy) ^ ROOK(emy);
    const uint64_t ebq = QUEEN(emy) ^ BISHOP(emy);
    uint64_t Ap = ALL_BOTH ^ ((1ULL << ip) | (1ULL << (eps - 8 * (2 * _cb.color - 1))));

    const uint64_t res  = mSb((plt::lBoard[kpos] & Ap)) | lSb((plt::rBoard[kpos] & Ap));
    const uint64_t tmp1 = mSb((plt::dlBoard[kpos] & Ap)) | lSb((plt::urBoard[kpos] & Ap));
    const uint64_t tmp2 = lSb((plt::ulBoard[kpos] & Ap)) | mSb((plt::drBoard[kpos] & Ap));

    if (res & erq) return false;
    // Board has to be invalid for this check
    if ((tmp1 | tmp2) & ebq) return false;

    return true;
}

static inline void
Add_shift_Pawns(uint64_t endSquares, const int shift, const chessBoard &_cb, MoveList &myMoves)
{
    if (!endSquares) return;

    if (myMoves.cpt_only)
    {
        uint64_t promotion_sq = endSquares & Rank18;
        endSquares = (endSquares & ALL(EMY)) | (promotion_sq);
        endSquares &= ALL(EMY);
    }

    while (endSquares)
    {
        const int fp = next_idx(endSquares);
        const int ip = fp + shift;

        const int mv_priority = gen_pawn_move_priority(_cb, ip, fp);
        const int move = encode_move(_cb, ip, fp, mv_priority);
        myMoves.Add(move);   
    }
}

static inline void
Add_m_Pawns(int ip, uint64_t endSquares, const chessBoard &_cb, MoveList &myMoves)
{
    if (!endSquares) return;

    if (myMoves.cpt_only) endSquares &= ALL(EMY);
    
    while (endSquares)
    {    
        const int fp = next_idx(endSquares);
        const int mv_priority = gen_move_priority(_cb, ip, fp);
        const int move = encode_move(_cb, ip, fp, mv_priority);

        myMoves.Add(move);
    }
}

static inline void
Add_pm_Pawns(int ip, uint64_t endSquares, const chessBoard &_cb, MoveList &myMoves)
{
    if (!endSquares) return;

    if (myMoves.cpt_only) endSquares &= ALL(EMY);
    
    myMoves.canPromote = true;
    
    while (endSquares)
    {    
        const int fp = next_idx(endSquares);
        const int mv_priority = gen_move_priority(_cb, ip, fp);
        const int move = encode_move(_cb, ip, fp, mv_priority);
        const int pr = (move >> 21);

        myMoves.Add(((pr + 12) << 21) ^ move ^ 0xC0000);
        myMoves.Add(((pr +  9) << 21) ^ move ^ 0x80000);
        myMoves.Add(((pr +  6) << 21) ^ move ^ 0x40000);
        myMoves.Add(((pr +  3) << 21) ^ move);
    }
}

#endif

#ifndef Piece_Movement

#ifndef PAWNS

static inline void
enpassant_pawns(const chessBoard &_cb, MoveList &myMoves,
    const uint64_t l_pawns, const uint64_t r_pawns, const int KA)
{
    const int own = _cb.color << 3;
    const int emy = own ^ 8;
    const int kpos = idx_no(KING(own));
    const int eps = _cb.csep & 127;
    const uint64_t _ep = 1ULL << eps;

    if (eps == 64 || (KA == 1 && !(plt::pcBoard[_cb.color][kpos] & PAWN(emy))))
        return;
    
    const auto shift = _cb.color == 1 ? l_shift : r_shift;

    if ((shift(l_pawns, 7 + (own >> 2)) & _ep) and en_passant_recheck(eps + (2 * emy - 9), _cb))
        Add_m_Pawns(eps + (2 * emy - 9), _ep, _cb, myMoves);

    if ((shift(r_pawns, 7 + (emy >> 2)) & _ep) and en_passant_recheck(eps + (2 * emy - 7), _cb))
        Add_m_Pawns(eps + (2 * emy - 7), _ep, _cb, myMoves);
}

static inline void
promotion_pawns(const chessBoard &_cb, MoveList &myMoves,
    const uint64_t move_sq, const uint64_t capt_sq, uint64_t pawns)
{
    while (pawns)
    {
        int __pos = next_idx(pawns);
        uint64_t res = ( plt::pBoard[_cb.color][__pos] & move_sq)
                     | (plt::pcBoard[_cb.color][__pos] & capt_sq);
        Add_pm_Pawns(__pos, res, _cb, myMoves);
    }
}


static inline void
pawn_movement(const chessBoard &_cb, MoveList &myMoves,
    const uint64_t pin_pieces, const int KA, const uint64_t atk_area)
{
    const uint64_t Rank27[2] = {Rank2, Rank7};
    const uint64_t Rank63[2] = {Rank6, Rank3};

    const auto shift = (_cb.color == 1) ? (l_shift) : (r_shift);

    const int own = _cb.color << 3;
    const int emy = own ^ 8;

    uint64_t pawns   = PAWN(own) & (~pin_pieces);
    uint64_t e_pawns = pawns & Rank27[_cb.color];
    uint64_t n_pawns = pawns ^ e_pawns;
    uint64_t l_pawns = n_pawns & RightAttkingPawns;
    uint64_t r_pawns = n_pawns &  LeftAttkingPawns;
    uint64_t s_pawns = (shift(n_pawns, 8) & (~ALL_BOTH)) & Rank63[_cb.color];

    uint64_t free_sq = ~ALL_BOTH;
    uint64_t enemyP  = ALL(emy);
    uint64_t capt_sq = (atk_area &  enemyP) * (KA) + ( enemyP) * (1 - KA);
    uint64_t move_sq = (atk_area ^ capt_sq) * (KA) + (free_sq) * (1 - KA);


    enpassant_pawns(_cb, myMoves, l_pawns, r_pawns, KA);
    promotion_pawns(_cb, myMoves, move_sq, capt_sq, e_pawns);

    Add_shift_Pawns(shift(l_pawns, 7 + (own >> 2)) & capt_sq, (2 * emy - 9), _cb, myMoves);
    Add_shift_Pawns(shift(r_pawns, 7 + (emy >> 2)) & capt_sq, (2 * emy - 7), _cb, myMoves);

    add_quiet_pawn_moves(shift(s_pawns, 8) & move_sq, (4 * emy - 16), _cb, myMoves);
    add_quiet_pawn_moves(shift(n_pawns, 8) & move_sq, (2 * emy -  8), _cb, myMoves);
}

#endif


static inline uint64_t
knight_legal_squares(int __pos, uint64_t _Op, uint64_t _Ap)
{ return ~_Op & knight_atk_sq(__pos, _Ap); }

static inline uint64_t
bishop_legal_squares(int __pos, uint64_t _Op, uint64_t _Ap)
{ return ~_Op & bishop_atk_sq(__pos, _Ap); }

static inline uint64_t
rook_legal_squares(int __pos, uint64_t _Op, uint64_t _Ap)
{ return ~_Op & rook_atk_sq(__pos, _Ap); }

static inline uint64_t
queen_legal_squares(int __pos, uint64_t _Op, uint64_t _Ap)
{ return ~_Op & queen_atk_sq(__pos, _Ap); }



static uint64_t
pinned_pieces_list(const chessBoard &_cb, MoveList &myMoves, const int KA)
{
    const int own = _cb.color << 3;
    const int emy = (_cb.color ^ 1) << 3;

    const int kpos = idx_no(KING(own));
    const uint64_t _Ap = ALL_BOTH;

    const uint64_t erq = QUEEN(emy) | ROOK(emy);
    const uint64_t ebq = QUEEN(emy) | BISHOP(emy);
    const uint64_t  rq = QUEEN(own) | ROOK(own);
    const uint64_t  bq = QUEEN(own) | BISHOP(own);

    uint64_t pinned_pieces = 0;

    if ((plt::line_Board[kpos] & erq) == 0 and
        (plt::diag_Board[kpos] & ebq) == 0) return 0ULL;
    

    const auto pins_check = [&] (const auto &__f, const uint64_t *table,
        uint64_t sliding_piece, uint64_t emy_sliding_piece, char pawn)
    {
        const uint64_t pieces = table[kpos] & _Ap;
        const uint64_t first_piece  = __f(pieces);
        const uint64_t second_piece = __f(pieces ^ first_piece);

        const int index_f = idx_no(first_piece);
        const int index_s = idx_no(second_piece);

        if (   !(first_piece  & ALL(own))
            or !(second_piece & emy_sliding_piece)) return;

        pinned_pieces |= first_piece;

        if (KA == 1) return;

        if ((first_piece & sliding_piece) != 0)
        {
            const uint64_t dest_sq = table[kpos] ^ table[index_s] ^ first_piece;
            return add_move_to_list(index_f, dest_sq, _cb, myMoves);
        }

        const uint64_t Rank63[2] = {Rank6, Rank3};

        const int eps    = _cb.csep & 127;
        const auto shift = (_cb.color == 1) ? (l_shift) : (r_shift);

        uint64_t n_pawn  = first_piece;
        uint64_t s_pawn  = (shift(n_pawn, 8) & (~ALL_BOTH)) & Rank63[_cb.color];
        uint64_t free_sq = ~ALL_BOTH;

        if (pawn == 's' and (first_piece & PAWN(own)))
        {
            uint64_t dest_sq = (shift(n_pawn, 8) | shift(s_pawn, 8)) & free_sq;

            //TODO: Replace below function for pawn-specific one.
            return add_move_to_list(index_f, dest_sq, _cb, myMoves);
        }

        if (pawn == 'c' and (first_piece & PAWN(own)))
        {
            uint64_t capt_sq = plt::pcBoard[_cb.color][index_f];
            uint64_t dest_sq = capt_sq & second_piece;

            if (eps != 64 && (table[kpos] & capt_sq & (1ULL << eps)) != 0)
                dest_sq |= 1ULL << eps;

            if ((dest_sq & Rank18) != 0)
                Add_pm_Pawns(index_f, dest_sq, _cb, myMoves);
            else
                add_move_to_list(index_f, dest_sq, _cb, myMoves);
        }
    };


    pins_check(lSb, plt::rBoard, rq, erq, '-');
    pins_check(mSb, plt::lBoard, rq, erq, '-');
    
    pins_check(lSb, plt::uBoard, rq, erq, 's');
    pins_check(mSb, plt::dBoard, rq, erq, 's');
    
    pins_check(lSb, plt::urBoard, bq, ebq, 'c');
    pins_check(lSb, plt::ulBoard, bq, ebq, 'c');
    
    pins_check(mSb, plt::drBoard, bq, ebq, 'c');
    pins_check(mSb, plt::dlBoard, bq, ebq, 'c');

    return pinned_pieces;
} 

static void
piece_movement(const chessBoard &_cb, MoveList &myMoves, const int KA)
{
    const uint64_t pinned_pieces = pinned_pieces_list(_cb, myMoves, KA);
    const uint64_t valid_sq = KA * _cb.Pieces[0] + (1 - KA) * AllSquares;

    const uint64_t own_pieces = ALL(OWN);
    const uint64_t all_pieces = ALL(WHITE) | ALL(BLACK);

    const auto Add_Vaild_Dest_Sq = [&] (const auto &__f, uint64_t piece)
    {
        piece &= ~pinned_pieces;
        while (piece != 0)
        {
            const int __pos = next_idx(piece);
            add_move_to_list(__pos,
                __f(__pos, own_pieces, all_pieces) & valid_sq, _cb, myMoves);
        }
    };

    pawn_movement(_cb, myMoves, pinned_pieces, KA, _cb.Pieces[0]);

    Add_Vaild_Dest_Sq(bishop_legal_squares, BISHOP(OWN));
    Add_Vaild_Dest_Sq(knight_legal_squares, KNIGHT(OWN));
    Add_Vaild_Dest_Sq(rook_legal_squares  , ROOK(OWN));
    Add_Vaild_Dest_Sq(queen_legal_squares , QUEEN(OWN));
}

#endif

#ifndef KING_MOVE_GENERATION

static uint64_t
generate_AttackedSquares(const chessBoard& _cb)
{
    uint64_t ans = 0;
    const uint64_t Apieces = ALL_BOTH ^ KING(OWN);

    const auto attacked_squares = [&] (const auto &__f, uint64_t piece)
    {
        uint64_t res = 0;
        while (piece)
            ans |= __f(next_idx(piece), Apieces);
        return res;
    };

    const int emy = EMY;

    ans |= pawn_atk_sq(_cb, 1);
    ans |= attacked_squares(bishop_atk_sq, BISHOP(emy));
    ans |= attacked_squares(knight_atk_sq, KNIGHT(emy));
    ans |= attacked_squares(rook_atk_sq  , ROOK(emy));
    ans |= attacked_squares(queen_atk_sq , QUEEN(emy));
    ans |= plt::KBoard[idx_no(KING(emy))];

    return ans;
}

static void
king_attackers(chessBoard &_cb)
{    
    const uint64_t attacked_sq = _cb.Pieces[8];

    if (attacked_sq && ((KING(OWN) & attacked_sq)) == 0)
    {
        _cb.KA = 0;
        return;
    }
    
    const int kpos = idx_no(KING(OWN));
    const int emy = EMY;
    const uint64_t _Ap = ALL_BOTH;
    int attk_count = 0;
    uint64_t attk_area = 0;

    const auto add_piece = [&] (const auto &__f, const uint64_t enemy)
    {
        const uint64_t area  = __f(kpos, _Ap);
        const uint64_t piece = area & enemy;

        if (piece == 0) return;

        if ((piece & (piece - 1)) != 0)
            ++attk_count;

        ++attk_count;
        attk_area |= (area & __f(idx_no(piece), _Ap)) | piece;
    };

    add_piece(rook_atk_sq  , QUEEN(emy) ^ ROOK(emy));
    add_piece(bishop_atk_sq, QUEEN(emy) ^ BISHOP(emy));
    add_piece(knight_atk_sq, KNIGHT(emy));

    const uint64_t pawn_sq = plt::pcBoard[_cb.color][kpos] & PAWN(emy);
    if (pawn_sq)
    {
        ++attk_count;
        attk_area |= pawn_sq;
    }


    _cb.KA = attk_count;
    _cb.Pieces[0] = attk_area;
}

static void
KingMoves(const chessBoard& _cb, MoveList& myMoves, const uint64_t Attacked_Sq)
{
    const auto add_castle_move = [&_cb, &myMoves] (const int ip, const int fp)
    {
        const int pt = 6;
        const int priority = 20 << 21;
        const int side = _cb.color << 20;
        const MoveType enc_move = priority + side + (pt << 12) + (fp << 6) + ip;
        myMoves.Add(enc_move);
    };

    const int kpos = idx_no(KING(OWN));
    const uint64_t K_sq = plt::KBoard[kpos];
    const uint64_t ans = K_sq & (~(ALL(OWN) | Attacked_Sq));

    add_move_to_list(kpos, ans, _cb, myMoves);

    if (!(_cb.csep & 1920) || ((1ULL << kpos) & Attacked_Sq)) return;

    const uint64_t Apieces = ALL_BOTH;
    const uint64_t covered_squares = Apieces | Attacked_Sq;

    const int shift         = 56 * (_cb.color ^ 1);
    const uint64_t l_mid_sq = 2ULL << shift;
    const uint64_t r_sq     = 96ULL << shift;
    const uint64_t l_sq     = 12ULL << shift;

    const uint64_t king_side  = 256 << (2 * _cb.color);
    const uint64_t queen_side = 128 << (2 * _cb.color);

    // Can castle king_side  and no pieces are in-between
    if ((_cb.csep & king_side) and !(r_sq & covered_squares))
        add_castle_move(kpos, 6 + shift);
    
    // Can castle queen_side and no pieces are in-between
    if ((_cb.csep & queen_side) and !(Apieces & l_mid_sq) and !(l_sq & covered_squares))
        add_castle_move(kpos, 2 + shift);
}


#endif

#ifndef LEGAL_MOVES_CHECK

static uint64_t
legal_pinned_pieces(const chessBoard& _cb)
{
    using std::make_pair;

    const int own = OWN;
    const int emy = EMY;

    const int kpos  = idx_no(KING(own));
    const uint64_t _Ap = ALL(WHITE) | ALL(BLACK);
    
    const uint64_t erq = QUEEN(emy) | ROOK(emy);
    const uint64_t ebq = QUEEN(emy) | BISHOP(emy);
    const uint64_t  rq = QUEEN(own) | ROOK(own);
    const uint64_t  bq = QUEEN(own) | BISHOP(own);
    uint64_t pinned_pieces = 0;

    const uint64_t ray_line = plt::line_Board[kpos];
    const uint64_t ray_diag = plt::diag_Board[kpos];
    
    if (!((ray_line & erq) | (ray_diag & ebq)))
        return 0;

    const auto can_pinned = [&] (const auto &__f, const auto *table,
            const uint64_t ownP, const uint64_t emyP, const char pawn) -> bool
    {
        const uint64_t pieces = table[kpos] & _Ap;
        const uint64_t first_piece  = __f(pieces);
        const uint64_t second_piece = __f(pieces ^ first_piece);

        const int index_f = idx_no(first_piece);
        const int index_s = idx_no(second_piece);

        if (  !(first_piece & ALL(own))
            or (!(second_piece & emyP))) return false;

        pinned_pieces |= first_piece;

        if (_cb.KA == 1) return false;

        if ((first_piece & ownP) != 0)
            return ((table[kpos] ^ table[index_s] ^ first_piece) != 0);


        const uint64_t Rank63[2] = {Rank6, Rank3};

        const int eps    = _cb.csep & 127;
        const auto shift = (_cb.color == 1) ? (l_shift) : (r_shift);

        uint64_t n_pawn  = first_piece;
        uint64_t s_pawn  = (shift(n_pawn, 8) & (~ALL_BOTH)) & Rank63[_cb.color];
        uint64_t free_sq = ~ALL_BOTH;

        if (pawn == 's' && (first_piece & PAWN(own)))
            return ((shift(n_pawn, 8) | shift(s_pawn, 8)) & free_sq) != 0;

        if (pawn == 'c' && (first_piece & PAWN(own)))
        {
            uint64_t capt_sq = plt::pcBoard[_cb.color][index_f];
            uint64_t dest_sq = capt_sq & second_piece;

            if (eps != 64 && (table[kpos] & capt_sq & (1ULL << eps)) != 0)
                dest_sq |= 1ULL << eps;

            return dest_sq;
        }

        return false;
    };

    if (   can_pinned(lSb, plt::rBoard,  rq, erq, '-')
        or can_pinned(mSb, plt::lBoard,  rq, erq, '-')
        or can_pinned(lSb, plt::uBoard,  rq, erq, 's')
        or can_pinned(mSb, plt::dBoard,  rq, erq, 's')) return 1;

    if (   can_pinned(lSb, plt::urBoard, bq, ebq, 'c')
        or can_pinned(lSb, plt::ulBoard, bq, ebq, 'c')
        or can_pinned(mSb, plt::drBoard, bq, ebq, 'c')
        or can_pinned(mSb, plt::dlBoard, bq, ebq, 'c')) return 1;

    return pinned_pieces;
}

static bool
legal_pawns_move(const chessBoard &_cb, const uint64_t pinned_pieces, const uint64_t atk_area)
{
    const uint64_t Rank27[2] = {Rank2, Rank7};
    const uint64_t Rank63[2] = {Rank6, Rank3};
    const auto shift = (_cb.color == 1) ? (l_shift) : (r_shift);

    const int own = _cb.color << 3;
    const int emy = own ^ 8;

    const auto legal_enpassant_pawns = [&] (uint64_t l_pawns, uint64_t r_pawns, int ep)
    {
        const int kpos = idx_no(KING(own));
        uint64_t ep_square = 1ULL << ep;

        if ((ep == 64) or (_cb.KA == 1 and !(plt::pcBoard[_cb.color][kpos] & PAWN(emy))))
            return false;

        return ((shift(l_pawns, 7 + (own >> 2)) & ep_square) and en_passant_recheck(ep + (2 * emy - 9), _cb))
            or ((shift(r_pawns, 7 + (emy >> 2)) & ep_square) and en_passant_recheck(ep + (2 * emy - 7), _cb));
    };

    const auto legal_promotion_pawns = [&] (uint64_t pawns, uint64_t move_sq, uint64_t capt_sq)
    {
        uint64_t valid_squares = 0;
        while (pawns)
        {
            const int __pos = next_idx(pawns);
            valid_squares |= ( plt::pBoard[_cb.color][__pos] & move_sq)
                           | (plt::pcBoard[_cb.color][__pos] & capt_sq);
        }

        return (valid_squares != 0);
    };

    
    uint64_t pawns   = PAWN(own) ^ (PAWN(own) & pinned_pieces);
    uint64_t e_pawns = pawns & Rank27[_cb.color];
    uint64_t n_pawns = pawns ^ e_pawns;
    uint64_t l_pawns = n_pawns & RightAttkingPawns;
    uint64_t r_pawns = n_pawns &  LeftAttkingPawns;
    uint64_t s_pawns = (shift(n_pawns, 8) & (~ALL_BOTH)) & Rank63[_cb.color];

    uint64_t free_sq = ~ALL_BOTH;
    uint64_t enemyP  = ALL(emy);
    uint64_t capt_sq = (atk_area &  enemyP) * (_cb.KA) + ( enemyP) * (1 - _cb.KA);
    uint64_t move_sq = (atk_area ^ capt_sq) * (_cb.KA) + (free_sq) * (1 - _cb.KA);


    if (legal_enpassant_pawns(l_pawns, r_pawns, _cb.csep & 127))
        return true;
    
    if (legal_promotion_pawns(e_pawns, move_sq, capt_sq))
        return true;

    uint64_t valid_squares = 0;
    
    // Capture Squares
    valid_squares |= shift(l_pawns, 7 + (own >> 2)) & capt_sq;
    valid_squares |= shift(r_pawns, 7 + (emy >> 2)) & capt_sq;

    // Non-Captures Squares
    valid_squares |= shift(s_pawns, 8) & move_sq;
    valid_squares |= shift(n_pawns, 8) & move_sq;

    return (valid_squares != 0);
}

static bool
legal_piece_move(const chessBoard &_cb)
{
    const auto pinned_pieces = legal_pinned_pieces(_cb);
    const auto filter = _cb.KA * _cb.Pieces[0] + (1 - _cb.KA) * AllSquares;


    if (pinned_pieces & 1)
        return true;

    const uint64_t  my_pieces = ALL(OWN);
    const uint64_t all_pieces = ALL(WHITE) | ALL(BLACK);

    const auto canMove = [&] (const auto &__f, uint64_t piece)
    {
        piece &= ~pinned_pieces;
        uint64_t legal_squares = 0;

        while (piece != 0)
            legal_squares |= __f(next_idx(piece), my_pieces, all_pieces) & filter;

        return (legal_squares != 0);
    };

    const int own = OWN;

    return legal_pawns_move(_cb, pinned_pieces, _cb.Pieces[0])
        or canMove(knight_legal_squares, KNIGHT(own))
        or canMove(bishop_legal_squares, BISHOP(own))
        or canMove(rook_legal_squares  ,   ROOK(own))
        or canMove(queen_legal_squares ,  QUEEN(own));
}

static bool
legal_king_move(const chessBoard& _cb, uint64_t attacked_squares)
{
    const int kpos = idx_no(KING(OWN));
    const uint64_t K_sq = plt::KBoard[kpos];

    const uint64_t legal_squares = K_sq & (~(ALL(OWN) | attacked_squares));

    // If no castling is possible or king is attacked by a enemy piece
    if (!(_cb.csep & 1920) or ((1ULL << kpos) & attacked_squares))
        return (legal_squares != 0);

    const uint64_t Apieces = ALL_BOTH;
    const uint64_t covered_squares = Apieces | attacked_squares;

    const int shift         = 56 * (_cb.color ^ 1);
    const uint64_t l_mid_sq = 2ULL << shift;
    const uint64_t r_sq     = 96ULL << shift;
    const uint64_t l_sq     = 12ULL << shift;

    const uint64_t king_side  = 256 << (2 * _cb.color);
    const uint64_t queen_side = 128 << (2 * _cb.color);
    
    // Legal moves possible if king can castle (king or queen) side
    // and no squares between king and rook is attacked by enemy piece
    // or occupied by own piece. 
    return ((_cb.csep &  king_side) and !(r_sq & covered_squares))
        or ((_cb.csep & queen_side) and !(Apieces & l_mid_sq) and !(l_sq & covered_squares));
}

#endif

#ifndef SEARCH_HELPER

bool
is_passad_pawn(int idx, chessBoard& _cb)
{
    if (_cb.color == 1)
    {
        uint64_t res = plt::uBoard[idx];
        if ((idx & 7) >= 1) res |= plt::uBoard[idx - 1];
        if ((idx & 7) <= 6) res |= plt::uBoard[idx + 1];
        if (res & PAWN(BLACK)) return false;
        return true;
    }

    uint64_t res = plt::dBoard[idx];
    if ((idx & 7) >= 1) res |= plt::dBoard[idx - 1];
    if ((idx & 7) <= 6) res |= plt::dBoard[idx + 1];
    if (res & PAWN(WHITE)) return false;
    return true;
}

bool
interesting_move(MoveType move, chessBoard& _cb)
{
    int ip = move & 63, fp = (move >> 6) & 63, ep = _cb.csep & 127;
    int pt = _cb.board[ip], cpt = _cb.board[fp];
    if (cpt) return true;
    if ((pt & 7) == 1) {
        if (fp == ep) return true;
        if (is_passad_pawn(ip, _cb)) return true;
    }
    if ((pt & 7) == 6 && std::abs(fp - ip) == 2) return true;

    _cb.MakeMove(move);
    bool res = in_check(_cb);
    _cb.UnmakeMove();

    return res;
}

bool
f_prune_move(MoveType move, chessBoard& _cb)
{
    int fp = (move >> 6) & 63;
    int cpt = _cb.board[fp];
    if (cpt) return true;
    if ((_cb.board[move & 63] & 7) == 1)
    {
        if (fp == (_cb.csep & 127)) return true;
        if (((1ULL << fp) & Rank18)) return true;
    }
    _cb.MakeMove(move);
    bool _in_check = in_check(_cb);
    _cb.UnmakeMove();
    if (_in_check) return true;

    return false;
}

#endif

#ifndef GENERATOR


bool
has_legal_moves(chessBoard& _cb)
{
    _cb.remove_movegen_extra_data();

    king_attackers(_cb);
    if ((_cb.KA < 2) and legal_piece_move(_cb))
        return true;

    _cb.Pieces[8] = generate_AttackedSquares(_cb);
    return legal_king_move(_cb, _cb.Pieces[8]);
}

MoveList
generate_moves(chessBoard& _cb, bool qs_only)
{
    //TODO: Add functionality to generate captures only

    /**
     * @brief 
     * enemy_attacked_sq => represents all the squares on board that are attacked by
     *                      an enemy piece, useful while generating legal moves for King
     * 
     * KA => represents how enemy pieces are checking our own king.
     * 
     * if KA == 0: king is not in check,
     *             legal king moves &
     *             any piece can move anywhere(excepts for pieces in pins)
     * 
     * if KA == 1: king is attacked by an enemy piece,
     *             legal king moves &
     *             piece can move only squares that prevents the king from being in check
     * 
     * if KA == 2: king is in check by at least two pieces,
     *             only legal king moves are possible
     * 
     * valid_moveable_sq => valid destination sqaures for pieces in case of (KA == 1)
     *
    **/

    MoveList myMoves;

    myMoves.cpt_only = qs_only;

    if (_cb.enemy_attacked_sq_generated() == false)
        _cb.Pieces[8] = generate_AttackedSquares(_cb);

    if (_cb.attackers_found() == false)
        king_attackers(_cb);

    if (_cb.KA < 2)
        piece_movement(_cb, myMoves, _cb.KA);
    
    KingMoves(_cb, myMoves, _cb.Pieces[8]);

    _cb.remove_movegen_extra_data();
    return myMoves;
}


#endif


/**
 * @brief Move Ordering Considerations
 * 
 * If Attacking higher value piece and not threatened by enemy piece -> Inc. priority
 * If threatened on ip -> Inc. priority
 * If threatened on fp -> dec. priority
 * 
 * capturing a hanging piece -> Inc. priority
 * 
 */

