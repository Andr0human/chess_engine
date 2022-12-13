
#include "movegen.h"


/**
 * @brief Add_pawn consideration
 * cap promotion still possible while pinned
 * 
 */


#ifndef MOVEGEN_UTILS

inline int
gen_move_priority(const chessBoard &__b, const int ip, const int fp)
{
    int res = 0;
    const int pt  =  __b.board[ip] & 7;
    const int cpt = -__b.board[fp] & 7;
    const int is_cpt = (bool)cpt;

    res += (cpt - pt + 12) * is_cpt;
    return res;
}

inline int
gen_pawn_move_priority(const chessBoard &__b, const int ip, const int fp)
{
    int res = 5;
    const int pt  = __b.board[ip] & 7;
    const int cpt = __b.board[fp] & 7;

    res += cpt ? (cpt - pt + 12) : 0;
    return res;
}

inline int
encode_move(const chessBoard &__b, const int ip, const int fp, const int pr)
{
    const int pt  = __b.board[ip] & 7;
    const int cpt = __b.board[fp] & 7;
    const int white_move = 1048576;
    
    int gen_move = __b.board[ip] > 8 ? white_move : 0;
    
    gen_move ^= (pr << 21) ^ (cpt << 15) ^ (pt << 12) ^ (fp << 6) ^ ip;
    return gen_move;
}

bool
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

inline void
Add_shift_Pawns(uint64_t endSquares, const int shift, const chessBoard &_cb, MoveList &myMoves)
{
    if (!endSquares) return;
    myMoves.canMove = true;

    if (myMoves.cpt_only)
    {
        uint64_t promotion_sq = endSquares & Rank18;
        endSquares = (endSquares & ALL(EMY)) | (promotion_sq);
        endSquares &= ALL(EMY);
    }

    while (endSquares) {
        const int fp = next_idx(endSquares);
        const int ip = fp + shift;

        const int mv_priority = gen_pawn_move_priority(_cb, ip, fp);
        const int move = encode_move(_cb, ip, fp, mv_priority);
        // myMoves.pMoves[myMoves.__count++] = move;
        myMoves.Add(move);   
    }
}

inline void
Add_m_Pawns(int ip, uint64_t endSquares, const chessBoard &_cb, MoveList &myMoves)
{
    if (!endSquares) return;
    myMoves.canMove = true;

    if (myMoves.cpt_only) endSquares &= ALL(EMY);
    
    while (endSquares)
    {    
        const int fp = next_idx(endSquares);
        const int mv_priority = gen_move_priority(_cb, ip, fp);
        const int move = encode_move(_cb, ip, fp, mv_priority);

        myMoves.Add(move);
    }
}

inline void
Add_pm_Pawns(int ip, uint64_t endSquares, const chessBoard &_cb, MoveList &myMoves)
{
    if (!endSquares) return;
    
    myMoves.canMove = true;
    if (myMoves.cpt_only) endSquares &= ALL(EMY);
    
    myMoves.canPromote = true;
    
    while (endSquares)
    {    
        const int fp = next_idx(endSquares);
        const int mv_priority = gen_move_priority(_cb, ip, fp);
        const int move = encode_move(_cb, ip, fp, mv_priority);
        const int pr = (move >> 21);
        
        // myMoves.pMoves[myMoves.__count++] = ((pr +  3) << 21) ^ move;
        // myMoves.pMoves[myMoves.__count++] = ((pr + 12) << 21) ^ move ^ 0xC0000;
        // myMoves.pMoves[myMoves.__count++] = ((pr +  9) << 21) ^ move ^ 0x80000;
        // myMoves.pMoves[myMoves.__count++] = ((pr +  6) << 21) ^ move ^ 0x40000;

        myMoves.Add(((pr + 12) << 21) ^ move ^ 0xC0000);
        myMoves.Add(((pr +  9) << 21) ^ move ^ 0x80000);
        myMoves.Add(((pr +  6) << 21) ^ move ^ 0x40000);
        myMoves.Add(((pr +  3) << 21) ^ move);
    }
}

#endif

#ifndef Piece_Movement

#ifndef PAWNS

inline void
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

    if ((shift(l_pawns, 7 + (own >> 2)) & _ep) && en_passant_recheck(eps + (2 * emy - 9), _cb))
        Add_m_Pawns(eps + (2 * emy - 9), _ep, _cb, myMoves);

    if ((shift(r_pawns, 7 + (emy >> 2)) & _ep) && en_passant_recheck(eps + (2 * emy - 7), _cb))
        Add_m_Pawns(eps + (2 * emy - 7), _ep, _cb, myMoves);
}

inline void
promotion_pawns(const chessBoard &_cb, MoveList &myMoves,
    const uint64_t move_sq, const uint64_t capt_sq, uint64_t pawns)
{
    while (pawns)
    {
        const int idx = next_idx(pawns);
        const uint64_t res = ( plt::pBoard[_cb.color][idx] & move_sq)
                           | (plt::pcBoard[_cb.color][idx] & capt_sq);
        Add_pm_Pawns(idx, res, _cb, myMoves);
    }
}


inline void
pawn_movement(const chessBoard &_cb, MoveList &myMoves,
    const uint64_t pin_pieces, const int KA, const uint64_t atk_area)
{
    const uint64_t Rank27[2] = {Rank2, Rank7};
    const uint64_t Rank63[2] = {Rank6, Rank3};

    const int own = _cb.color << 3;
    const int emy = own ^ 8;

    const auto pawns   = PAWN(own) ^ (PAWN(own) & pin_pieces);
    const auto e_pawns = pawns & Rank27[_cb.color];
    const auto n_pawns = pawns ^ e_pawns;
    const auto l_pawns = n_pawns & RightAttkingPawns;
    const auto r_pawns = n_pawns &  LeftAttkingPawns;
    
    const auto free_sq = ~ALL_BOTH;
    const auto enemyP  = ALL(emy);
    const auto capt_sq = (atk_area &  enemyP) * (KA) + ( enemyP) * (1 - KA);
    const auto move_sq = (atk_area ^ capt_sq) * (KA) + (free_sq) * (1 - KA);

    const auto shift   = _cb.color == 1 ? l_shift : r_shift;
    const auto s_pawns = (shift(n_pawns, 8) & free_sq) & Rank63[_cb.color];

    enpassant_pawns(_cb, myMoves, l_pawns, r_pawns, KA);
    promotion_pawns(_cb, myMoves, move_sq, capt_sq, e_pawns);

    Add_shift_Pawns(shift(l_pawns, 7 + (own >> 2)) & capt_sq, (2 * emy - 9), _cb, myMoves);
    Add_shift_Pawns(shift(r_pawns, 7 + (emy >> 2)) & capt_sq, (2 * emy - 7), _cb, myMoves);

    add_quiet_pawn_moves(shift(s_pawns, 8) & move_sq, (4 * emy - 16), _cb, myMoves);
    add_quiet_pawn_moves(shift(n_pawns, 8) & move_sq, (2 * emy -  8), _cb, myMoves);
}

#endif


inline uint64_t
KnightMovement(int idx, const chessBoard& _cb)
{
    const uint64_t dest_sqaures = knight_atk_sq(idx, 0);
    return dest_sqaures ^ (dest_sqaures & ALL(OWN));
}

inline uint64_t
BishopMovement(int idx, const chessBoard& _cb)
{
    const uint64_t dest_sqaures = bishop_atk_sq(idx, ALL_BOTH);
    return dest_sqaures ^ (dest_sqaures & ALL(OWN));
}

inline uint64_t
RookMovement(int idx, const chessBoard& _cb)
{
    const uint64_t dest_sqaures = rook_atk_sq(idx, ALL_BOTH);
    return dest_sqaures ^ (dest_sqaures & ALL(OWN));
}

inline uint64_t
QueenMovement(int idx, const chessBoard &_cb)
{
    const uint64_t dest_sqaures = queen_atk_sq(idx, ALL_BOTH);
    return dest_sqaures ^ (dest_sqaures & ALL(OWN));
}

uint64_t
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

    if ((plt::line_Board[kpos] & erq) == 0 &&
        (plt::diag_Board[kpos] & ebq) == 0) return 0ULL;
    

    const auto pins_check = [&] (const auto &__f, const uint64_t *table,
            uint64_t ownP, uint64_t emyP, char pawn)
    {
        const uint64_t pieces = table[kpos] & _Ap;
        const uint64_t first_piece = __f(pieces);
        const uint64_t second_piece = __f(pieces ^ first_piece);

        const int index_f = idx_no(first_piece);
        const int index_s = idx_no(second_piece);

        if ((first_piece & ALL(own)) == 0 ||
            (second_piece & emyP) == 0) return;

        pinned_pieces |= first_piece;

        if (KA == 1) return;

        if ((first_piece & ownP) != 0)
        {
            const uint64_t dest_sq = table[kpos] ^ table[index_s] ^ first_piece;
            add_move_to_list(index_f, dest_sq, _cb, myMoves);
        }
        
        const int color = _cb.color;
        const int side  = 2 * color - 1;
        const int eps   = _cb.csep & 127;

        if (pawn == 's' && (first_piece & PAWN(own)))
        {
            uint64_t dest_sq = plt::pBoard[color][index_f] & (~_Ap);
            uint64_t second_rank = color == 1 ? Rank2 : Rank7;

            if (dest_sq && (first_piece & second_rank))
                dest_sq |= plt::pBoard[color][index_f + 8 * side] & (~_Ap);
            
            add_move_to_list(index_f, dest_sq, _cb, myMoves);
        }

        if (pawn == 'c' && (first_piece & PAWN(own)))
        {
            uint64_t capt_sq = plt::pcBoard[color][index_f];
            uint64_t dest_sq = capt_sq & second_piece;

            if (eps != 64 && (table[kpos] & capt_sq & (1ULL << eps)) != 0)
                dest_sq |= 1ULL << eps;

            if ((dest_sq & Rank18) != 0)
                Add_pm_Pawns(index_f, dest_sq, _cb, myMoves);
            else
                add_move_to_list(index_f, dest_sq, _cb, myMoves);
        }

        return;
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

void
piece_movement(const chessBoard &_cb, MoveList &myMoves, const int KA)
{
    const uint64_t pinned_pieces = pinned_pieces_list(_cb, myMoves, KA);
    const uint64_t valid_sq = KA * _cb.Pieces[0] + (1 - KA) * AllSquares;

    const auto Add_Vaild_Dest_Sq = [&] (const auto &__f, uint64_t piece)
    {
        piece ^= (piece & pinned_pieces);
        while (piece != 0)
        {
            const int __pos = next_idx(piece);
            add_move_to_list(__pos, __f(__pos, _cb) & valid_sq, _cb, myMoves);
        }
    };

    const int own = OWN;
    pawn_movement(_cb, myMoves, pinned_pieces, KA, _cb.Pieces[0]);

    Add_Vaild_Dest_Sq(BishopMovement, BISHOP(own));
    Add_Vaild_Dest_Sq(KnightMovement, KNIGHT(own));
    Add_Vaild_Dest_Sq(RookMovement  , ROOK(own)  );
    Add_Vaild_Dest_Sq(QueenMovement , QUEEN(own) );
}

#endif

#ifndef KING_MOVE_GENERATION

uint64_t
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

void
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

void
KingMoves(const chessBoard& _cb, MoveList& myMoves, const uint64_t Attacked_Sq)
{
    const auto add_castle_move = [&_cb, &myMoves] (const int ip, const int fp)
    {
        const int pt = 6;
        const int priority = 20 << 21;
        const int side = _cb.color << 20;
        const int enc_move = priority + side + (pt << 12) + (fp << 6) + ip;
        myMoves.Add(enc_move);
    };

    const int kpos = idx_no(KING(OWN));
    const uint64_t K_sq = plt::KBoard[kpos];
    const uint64_t ans = K_sq ^ (K_sq & (ALL(OWN) | Attacked_Sq));

    add_move_to_list(kpos, ans, _cb, myMoves);

    if (!(_cb.csep & 1920) || ((1ULL << kpos) & Attacked_Sq)) return;

    const uint64_t Apieces = ALL_BOTH;
    const uint64_t opd = Apieces | Attacked_Sq;

    const uint64_t sq_5_6 = 96ULL;
    const uint64_t sq_57 = 144115188075855872ULL;
    const uint64_t sq_58_59 = 864691128455135232ULL;
    const uint64_t sq_61_62 = 6917529027641081856ULL;

    if (_cb.color == 1)
    {
        if ((_cb.csep & 1024) && !(sq_5_6 & opd))
            add_castle_move(kpos, 6);
        if ((_cb.csep & 512) && (!(Apieces & 2) && !(12 & opd)))
            add_castle_move(kpos, 2);
    }
    else
    {
        if ((_cb.csep & 256) && !(opd & sq_61_62))
            add_castle_move(kpos, 62);
        if ((_cb.csep & 128) && (!(Apieces & sq_57) && !(opd & sq_58_59)))
            add_castle_move(kpos, 58);
    }
}


#endif

#ifndef LEGAL_MOVES_CHECK

uint64_t
can_pinned_pieces_move(const chessBoard& _cb, const int KA)
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
        const uint64_t first_piece = __f(pieces);
        const uint64_t second_piece = __f(pieces ^ first_piece);

        const int index_f = idx_no(first_piece);
        const int index_s = idx_no(second_piece);

        if ((first_piece & ALL(own)) == 0 ||
            (second_piece & emyP) == 0) return false;

        pinned_pieces |= first_piece;

        if (KA == 1) return false;

        if ((first_piece & ownP) != 0)
        {
            const uint64_t dest_sq = table[kpos] ^ table[index_s] ^ first_piece;
            return dest_sq != 0;
        }

        const int color = _cb.color;
        const int side  = 2 * color - 1;
        const int eps   = _cb.csep & 127;

        if (pawn == 's' && (first_piece & PAWN(own)))
        {
            uint64_t dest_sq = plt::pBoard[color][index_f] & (~_Ap);
            uint64_t second_rank = color == 1 ? Rank2 : Rank7;

            if (dest_sq && (first_piece & second_rank))
                dest_sq |= plt::pBoard[color][index_f + 8 * side] & (~_Ap);
            
            return dest_sq != 0;
        }

        if (pawn == 'c' && (first_piece & PAWN(own)))
        {
            uint64_t capt_sq = plt::pcBoard[color][index_f];
            uint64_t dest_sq = capt_sq & second_piece;

            if (eps != 64 && (table[kpos] & capt_sq & (1ULL << eps)) != 0)
                dest_sq |= 1ULL << eps;

            return dest_sq;
        }

        return false;
    };

    if (can_pinned(lSb, plt::rBoard,  rq, erq, '-')) return 1;
    if (can_pinned(mSb, plt::lBoard,  rq, erq, '-')) return 1;
    if (can_pinned(lSb, plt::uBoard,  rq, erq, 's')) return 1;
    if (can_pinned(mSb, plt::dBoard,  rq, erq, 's')) return 1;
    
    if (can_pinned(lSb, plt::urBoard, bq, ebq, 'c')) return 1;
    if (can_pinned(lSb, plt::ulBoard, bq, ebq, 'c')) return 1;
    if (can_pinned(mSb, plt::drBoard, bq, ebq, 'c')) return 1;
    if (can_pinned(mSb, plt::dlBoard, bq, ebq, 'c')) return 1;

    return pinned_pieces;
}

bool
can_pawns_move(const chessBoard &_cb, const uint64_t pinned_pieces,
    const int KA, const uint64_t atk_area)
{
    const int own = OWN;
    const int emy = own ^ 8;

    const uint64_t pawns = PAWN(own) ^ pinned_pieces;
    const uint64_t _Ap = ALL_BOTH, free_sq = ~_Ap;
    const int eps = _cb.csep & 127;
    const uint64_t _ep = 1ULL << eps;
    const int kpos = idx_no(KING(own));

    const uint64_t capt_sq = KA == 0 ? ALL(emy) : atk_area & ALL(emy);
    const uint64_t move_sq = atk_area ^ capt_sq;

    const uint64_t l_pawns = PAWN(own) & RightAttkingPawns;
    const uint64_t r_pawns = PAWN(own) &  LeftAttkingPawns;

    
    if (_cb.color == 1)
    {
        if (KA == 0 || (KA == 1 && (plt::pcBoard[1][kpos] & capt_sq)))
        {
            if (((l_pawns << 9) & _ep) && en_passant_recheck(eps - 9, _cb))
                return true;

            if (((r_pawns << 7) & _ep) && en_passant_recheck(eps - 7, _cb))
                return true;
        }

        if ((l_pawns << 9) & capt_sq) return true;
        if ((r_pawns << 7) & capt_sq) return true;

        const uint64_t flask = (pawns << 8) & free_sq;
        const uint64_t s_pawns = (flask >> 8) & Rank2;

        if (KA == 0 && flask) return true;
        if (KA == 1 && flask)
        {
            if (flask & move_sq) return true;
            if ((s_pawns << 16) & move_sq) return true;
        }

        return false;
    }

    if (KA == 0 || (KA == 1 && (plt::pcBoard[0][kpos] & capt_sq)))
    {
        if (((l_pawns >> 7) & _ep) && en_passant_recheck(eps + 7, _cb))
            return true;

        if (((r_pawns >> 9) & _ep) && en_passant_recheck(eps + 9, _cb))
            return true;
    }

    if ((l_pawns >> 7) & capt_sq) return true;
    if ((r_pawns >> 9) & capt_sq) return true;

    const uint64_t flask = (pawns >> 8) & free_sq;
    const uint64_t s_pawns = (flask << 8) & Rank7;

    if (KA == 0 && flask) return true;
    if (KA == 1 && flask)
    {
        if (flask & move_sq) return true;
        if ((s_pawns >> 16) & move_sq) return true;
    }

    return false;
}

bool
can_piece_move(const chessBoard &_cb, const int KA)
{
    const auto pinned_pieces = can_pinned_pieces_move(_cb, KA);
    const auto filter = KA * _cb.Pieces[0] + (1 - KA) * AllSquares;

    bool legal = static_cast<bool>(pinned_pieces & 1);

    if (legal) return true;

    const auto canMove = [&] (const auto &__f, uint64_t piece)
    {
        piece = piece ^ (piece & pinned_pieces);

        while (piece != 0) {
            const int __pos = next_idx(piece);
            if (__f(__pos, _cb) & filter) return true;
        }

        return false;
    };

    const int own = OWN;

    legal = legal ? true : can_pawns_move(_cb, pinned_pieces, KA, _cb.Pieces[0]);
    legal = legal ? true : canMove(KnightMovement, KNIGHT(own));
    legal = legal ? true : canMove(BishopMovement, BISHOP(own));
    legal = legal ? true : canMove(RookMovement  ,   ROOK(own));
    legal = legal ? true : canMove(QueenMovement ,  QUEEN(own));

    return legal;
}

uint64_t
canKingMove(const chessBoard& _cb, uint64_t Attacked_Squares)
{
    const uint64_t _Ap = ALL_BOTH;
    const int kpos = idx_no(KING(OWN));
    uint64_t ans, Ksquares = plt::KBoard[kpos], opd = _Ap | Attacked_Squares;
    ans = Ksquares ^ (Ksquares & (ALL(OWN) | Attacked_Squares));
    
    if (ans) return 1;
    if (!(_cb.csep & 1920)) return 0;

    if (_cb.color == 1 && !((1ULL << kpos) & Attacked_Squares))
    {
        if (_cb.csep & 1024)
            if (!(96 & opd)) return 1;
        if (_cb.csep & 512)
            if (!(_Ap & 2) && !(12 & opd)) return 1;
    }
    if (_cb.color == 0 && !((1ULL << kpos) & Attacked_Squares))
    {
        if (_cb.csep & 256)
            if (!(opd & 6917529027641081856ULL)) return 1;
        if (_cb.csep & 128)
            if (!(_Ap & 144115188075855872ULL) && !(opd & 864691128455135232ULL)) return 1;
    }

    return 0;
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
interesting_move(int move, chessBoard& _cb)
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
    bool res = Incheck(_cb);
    _cb.UnmakeMove();

    return res;
}

bool
f_prune_move(int move, chessBoard& _cb)
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
    bool _in_check = Incheck(_cb);
    _cb.UnmakeMove();
    if (_in_check) return true;

    return false;
}

#endif

#ifndef GENERATOR

/* 
MoveList generateMoves(const chessBoard &board, bool qs_only) {
    
    MoveList myMoves;

    uint64_t Attacked_sq = generate_AttackedSquares(board);          // Generate all squares attacked by opp. pieces
    KAinfo ka_pieces = findkingAttackers(board, Attacked_sq);   // Find all opp. pieces attacking our king

    myMoves.set(board.color, ka_pieces.attackers, qs_only);    // Store all extra info besides legal moves

    if (ka_pieces.attackers == 0)                               // Gen. all legal moves if king is not in check
        KA0_pieceMovement(board, myMoves);

    if (ka_pieces.attackers == 1)                               // Gen. all legal moves if king is in check
        KA1_pieceMovement(board, ka_pieces, myMoves);

    KingMoves(board, myMoves, Attacked_sq);                     // Gen. all legal moves for King
    return myMoves;

}
*/


// Returns true if board position has at least one legal move.
bool
has_legal_moves(chessBoard &_cb)
{
    _cb.remove_movegen_extra_data();

    // _cb.Pieces[8] = generate_AttackedSquares(_cb);
    // if (canKingMove(_cb, _cb.Pieces[8])) return true;
    // king_attackers(_cb);
    // if (_cb.KA < 2 && can_piece_move(_cb, _cb.KA)) return true;
    // return false;

    king_attackers(_cb);
    if (_cb.KA < 2 && can_piece_move(_cb, _cb.KA)) return true;

    _cb.Pieces[8] = generate_AttackedSquares(_cb);
    return canKingMove(_cb, _cb.Pieces[8]);
}

MoveList
generate_moves(chessBoard &_cb, bool qs_only)
{
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

    // const auto enemy_attacked_sq = (_cb.Pieces[8] == 0) ?
    //            generate_AttackedSquares(_cb) : _cb.Pieces[8];
    // _cb.Pieces[8] = enemy_attacked_sq;

    // if (_cb.Pieces[8] == 0)
    //     _cb.Pieces[8] = generate_AttackedSquares(_cb);

    // const auto &[KA, valid_moveable_sq] = king_attackers(_cb);
    // _cb.Pieces[0] = valid_moveable_sq;
    
    // _cb.KA = KA;
    // if (KA < 2) piece_movement(_cb, myMoves, KA);
    // KingMoves(_cb, myMoves, enemy_attacked_sq);
    
    // _cb.Pieces[0] = _cb.Pieces[8] = 0;
    // return myMoves;


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

