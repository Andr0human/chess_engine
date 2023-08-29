
#include "movegen.h"


/**
 * @brief Add_pawn consideration
 * cap promotion still possible while pinned
 * 
**/


#ifndef MOVEGEN_UTILS

static inline Move
gen_move_priority(const ChessBoard& __b, Square ip, Square fp)
{
    int res = 0;
    PieceType ipt = type_of(__b.piece_on_square(ip));
    PieceType fpt = type_of(__b.piece_on_square(fp));
    int is_cpt = (fpt != 0);

    res += (fpt - ipt + 16) * is_cpt;
    return res;
}

static inline Move
gen_pawn_move_priority(const ChessBoard& __b, Square ip, Square fp)
{
    int res = 5;

    PieceType ipt = type_of(__b.piece_on_square(ip));
    PieceType fpt = type_of(__b.piece_on_square(fp));

    res += fpt ? (fpt - ipt + 16) : 0;
    return res;
}

static inline Move
encode_move(const ChessBoard& b, Square ip, Square fp, int pr)
{
    PieceType ipt = type_of(b.piece_on_square(ip));
    PieceType fpt = type_of(b.piece_on_square(fp));

    int color_bit = int(b.color) << 20;
    Move gen_move =
        color_bit | (pr << 21) | (fpt << 15) | (ipt << 12) | (fp << 6) | ip;
    return gen_move;
}

static bool
en_passant_recheck(Square ip, const ChessBoard& _cb)
{
    Color color = _cb.color;

    Square kpos = idx_no( _cb.piece(color, KING) );
    Square eps = _cb.csep & 127;

    Bitboard erq = _cb.piece(~color, QUEEN) | _cb.piece(~color, ROOK);
    Bitboard ebq = _cb.piece(~color, QUEEN) | _cb.piece(~color, BISHOP);
    Bitboard  Ap = _cb.All() ^ ((1ULL << ip) | (1ULL << (eps - 8 * (2 * color - 1))));

    Bitboard res  = mSb((plt::LeftMasks[kpos] & Ap)) | lSb((plt::RightMasks[kpos] & Ap));
    Bitboard tmp1 = mSb((plt::DownLeftMasks[kpos] & Ap)) | lSb((plt::UpRightMasks[kpos] & Ap));
    Bitboard tmp2 = lSb((plt::UpLeftMasks[kpos] & Ap)) | mSb((plt::DownRightMasks[kpos] & Ap));

    if (res & erq) return false;
    // Board has to be invalid for this check
    if ((tmp1 | tmp2) & ebq) return false;

    return true;
}

static inline void
Add_shift_Pawns(Bitboard endSquares, int shift, const ChessBoard &_cb, MoveList &myMoves)
{
    if (!endSquares) return;

    Color c_emy = ~_cb.color;
    Bitboard emy_pieces = _cb.piece(c_emy, PieceType::ALL);

    if (myMoves.cpt_only)
    {
        Bitboard promotion_sq = endSquares & Rank18;
        endSquares = (endSquares & emy_pieces) | (promotion_sq);
        endSquares &= emy_pieces;
    }

    while (endSquares)
    {
        Square fp = next_idx(endSquares);
        Square ip = fp + shift;

        int mv_priority = gen_pawn_move_priority(_cb, ip, fp);
        Move move = encode_move(_cb, ip, fp, mv_priority);
        myMoves.Add(move);   
    }
}

static inline void
Add_m_Pawns(Square ip, Bitboard endSquares, const ChessBoard &_cb, MoveList &myMoves)
{
    if (!endSquares) return;

    Bitboard emy_pieces = _cb.piece(~_cb.color, PieceType::ALL);
    if (myMoves.cpt_only) endSquares &= emy_pieces;

    while (endSquares)
    {    
        Square fp = next_idx(endSquares);
        int mv_priority = gen_move_priority(_cb, ip, fp);
        Move move = encode_move(_cb, ip, fp, mv_priority);

        myMoves.Add(move);
    }
}

static inline void
Add_pm_Pawns(Square ip, Bitboard endSquares, const ChessBoard &_cb, MoveList &myMoves)
{
    if (!endSquares) return;

    Bitboard emy_pieces = _cb.piece(~_cb.color, PieceType::ALL);
    if (myMoves.cpt_only) endSquares &= emy_pieces;
    
    myMoves.canPromote = true;
    
    while (endSquares != 0)
    {    
        Square fp = next_idx(endSquares);
        int mv_priority = gen_move_priority(_cb, ip, fp);
        Move move = encode_move(_cb, ip, fp, mv_priority);
        int pr = (move >> 21);

        myMoves.Add(((pr + 12) << 21) ^ move ^ 0xC0000);
        myMoves.Add(((pr +  9) << 21) ^ move ^ 0x80000);
        myMoves.Add(((pr +  6) << 21) ^ move ^ 0x40000);
        myMoves.Add(((pr +  3) << 21) ^ move);
    }
}

bool
legal_move_for_position(Move move, ChessBoard& pos)
{
    Square ip = move & 63;
    Square fp = (move >> 6) & 63;

    Piece ipt = pos.piece_on_square(ip);
    Piece fpt = pos.piece_on_square(fp);

    Color side = pos.color;

    // If no piece on init_sq or piece_color != side_to_move
    if (((ipt & 7) == 0) or (((ipt >> 3) & 1) != side))
        return false;

    // If capt_piece_color == side_to_move
    if ((fpt != 0) and ((fpt >> 3) & 1) == side)
        return false;

    MoveList myMoves = generate_moves(pos, false);

    for (Move legal_move : myMoves.pMoves)
        if (move == legal_move) return true;
    
    return false;
}

#endif

#ifndef Piece_Movement

#ifndef PAWNS

static inline void
enpassant_pawns(const ChessBoard &_cb, MoveList &myMoves,
    Bitboard l_pawns, Bitboard r_pawns, int KA)
{
    Color c_my = _cb.color;
    int own =  c_my << 3;
    int emy = ~c_my << 3;

    Square kpos = idx_no( _cb.piece(c_my, KING) );
    Square eps = _cb.csep & 127;
    Bitboard _ep = 1ULL << eps;

    if (eps == 64 || (KA == 1 && !(plt::PawnCaptureMasks[c_my][kpos] & _cb.piece(~c_my, PAWN))))
        return;

    const auto shift = c_my == Color::WHITE ? l_shift : r_shift;

    if ((shift(l_pawns, 7 + (own >> 2)) & _ep) and en_passant_recheck(eps + (2 * emy - 9), _cb))
        Add_m_Pawns(eps + (2 * emy - 9), _ep, _cb, myMoves);

    if ((shift(r_pawns, 7 + (emy >> 2)) & _ep) and en_passant_recheck(eps + (2 * emy - 7), _cb))
        Add_m_Pawns(eps + (2 * emy - 7), _ep, _cb, myMoves);
}

static inline void
promotion_pawns(const ChessBoard &_cb, MoveList &myMoves,
    Bitboard move_sq, Bitboard capt_sq, Bitboard pawns)
{
    while (pawns)
    {
        Square __pos = next_idx(pawns);
        Bitboard res = (plt::PawnMasks[_cb.color][__pos] & move_sq)
                     | (plt::PawnCaptureMasks[_cb.color][__pos] & capt_sq);
        Add_pm_Pawns(__pos, res, _cb, myMoves);
    }
}


static inline void
pawn_movement(const ChessBoard &_cb, MoveList &myMoves,
    Bitboard pin_pieces, int KA, Bitboard atk_area)
{
    Bitboard Rank27[2] = {Rank2, Rank7};
    Bitboard Rank63[2] = {Rank6, Rank3};

    const auto shift = (_cb.color == 1) ? (l_shift) : (r_shift);

    Color c_my  = _cb.color;
    Color c_emy = ~_cb.color;
    int own = int(c_my) << 3;
    int emy = int(c_emy) << 3;

    Bitboard pawns   = _cb.piece(c_my, PieceType::PAWN) & (~pin_pieces);
    Bitboard e_pawns = pawns & Rank27[_cb.color];
    Bitboard n_pawns = pawns ^ e_pawns;
    Bitboard l_pawns = n_pawns & RightAttkingPawns;
    Bitboard r_pawns = n_pawns &  LeftAttkingPawns;
    Bitboard s_pawns = (shift(n_pawns, 8) & (~_cb.All())) & Rank63[_cb.color];

    Bitboard free_sq = ~_cb.All();
    Bitboard enemyP  = _cb.piece(c_emy, PieceType::ALL);
    Bitboard capt_sq = (atk_area &  enemyP) * (KA) + ( enemyP) * (1 - KA);
    Bitboard move_sq = (atk_area ^ capt_sq) * (KA) + (free_sq) * (1 - KA);


    enpassant_pawns(_cb, myMoves, l_pawns, r_pawns, KA);
    promotion_pawns(_cb, myMoves, move_sq, capt_sq, e_pawns);

    Add_shift_Pawns(shift(l_pawns, 7 + (own >> 2)) & capt_sq, (2 * emy - 9), _cb, myMoves);
    Add_shift_Pawns(shift(r_pawns, 7 + (emy >> 2)) & capt_sq, (2 * emy - 7), _cb, myMoves);

    add_quiet_pawn_moves(shift(s_pawns, 8) & move_sq, (4 * emy - 16), _cb, myMoves);
    add_quiet_pawn_moves(shift(n_pawns, 8) & move_sq, (2 * emy -  8), _cb, myMoves);
}

#endif


static inline Bitboard
knight_legal_squares(Square __pos, Bitboard _Op, Bitboard _Ap)
{ return ~_Op & knight_atk_sq(__pos, _Ap); }

static inline Bitboard
bishop_legal_squares(Square __pos, Bitboard _Op, Bitboard _Ap)
{ return ~_Op & bishop_atk_sq(__pos, _Ap); }

static inline Bitboard
rook_legal_squares(Square __pos, Bitboard _Op, Bitboard _Ap)
{ return ~_Op & rook_atk_sq(__pos, _Ap); }

static inline Bitboard
queen_legal_squares(Square __pos, Bitboard _Op, Bitboard _Ap)
{ return ~_Op & queen_atk_sq(__pos, _Ap); }



static Bitboard
pinned_pieces_list(const ChessBoard& _cb, MoveList &myMoves, int KA)
{
    Color c_my  =  _cb.color;
    Color c_emy = ~_cb.color;

    Square kpos = idx_no( _cb.piece(c_my, KING) );
    Bitboard _Ap = _cb.All();

    Bitboard erq = _cb.piece(c_emy, QUEEN) | _cb.piece(c_emy, ROOK  );
    Bitboard ebq = _cb.piece(c_emy, QUEEN) | _cb.piece(c_emy, BISHOP);
    Bitboard  rq = _cb.piece(c_my , QUEEN) | _cb.piece(c_my , ROOK  );
    Bitboard  bq = _cb.piece(c_my , QUEEN) | _cb.piece(c_my , BISHOP);

    Bitboard pinned_pieces = 0;

    if ((    plt::LineMasks[kpos] & erq) == 0 and
        (plt::DiagonalMasks[kpos] & ebq) == 0) return 0ULL;
    

    const auto pins_check = [&] (const auto &__f, Bitboard *table,
        Bitboard sliding_piece, Bitboard emy_sliding_piece, char pawn)
    {
        Bitboard pieces = table[kpos] & _Ap;
        Bitboard first_piece  = __f(pieces);
        Bitboard second_piece = __f(pieces ^ first_piece);

        Square index_f = idx_no(first_piece);
        Square index_s = idx_no(second_piece);

        if (   !(first_piece  & _cb.piece(c_my, PieceType::ALL))
            or !(second_piece & emy_sliding_piece)) return;

        pinned_pieces |= first_piece;

        if (KA == 1) return;

        if ((first_piece & sliding_piece) != 0)
        {
            Bitboard dest_sq = table[kpos] ^ table[index_s] ^ first_piece;
            return add_move_to_list(index_f, dest_sq, _cb, myMoves);
        }

        Bitboard Rank63[2] = {Rank6, Rank3};

        Square eps    = _cb.csep & 127;
        const auto shift = (_cb.color == 1) ? (l_shift) : (r_shift);

        Bitboard n_pawn  = first_piece;
        Bitboard s_pawn  = (shift(n_pawn, 8) & (~_cb.All())) & Rank63[_cb.color];
        Bitboard free_sq = ~_cb.All();

        if (pawn == 's' and (first_piece & _cb.piece(c_my, PieceType::PAWN)))
        {
            Bitboard dest_sq = (shift(n_pawn, 8) | shift(s_pawn, 8)) & free_sq;

            //! TODO: Replace below function for pawn-specific one.
            return add_move_to_list(index_f, dest_sq, _cb, myMoves);
        }

        if (pawn == 'c' and (first_piece & _cb.piece(c_my, PieceType::PAWN)))
        {
            Bitboard capt_sq = plt::PawnCaptureMasks[_cb.color][index_f];
            Bitboard dest_sq = capt_sq & second_piece;

            if (eps != 64 && (table[kpos] & capt_sq & (1ULL << eps)) != 0)
                dest_sq |= 1ULL << eps;

            if ((dest_sq & Rank18) != 0)
                Add_pm_Pawns(index_f, dest_sq, _cb, myMoves);
            else
                add_move_to_list(index_f, dest_sq, _cb, myMoves);
        }
    };


    pins_check(lSb, plt::RightMasks, rq, erq, '-');
    pins_check(mSb, plt::LeftMasks, rq, erq, '-');
    
    pins_check(lSb, plt::UpMasks, rq, erq, 's');
    pins_check(mSb, plt::DownMasks, rq, erq, 's');
    
    pins_check(lSb, plt::UpRightMasks, bq, ebq, 'c');
    pins_check(lSb, plt::UpLeftMasks, bq, ebq, 'c');
    
    pins_check(mSb, plt::DownRightMasks, bq, ebq, 'c');
    pins_check(mSb, plt::DownLeftMasks, bq, ebq, 'c');

    return pinned_pieces;
} 

static void
piece_movement(const ChessBoard &_cb, MoveList &myMoves, int KA)
{
    Bitboard pinned_pieces = pinned_pieces_list(_cb, myMoves, KA);
    Bitboard valid_sq = KA * _cb.legal_squares_mask + (1 - KA) * AllSquares;

    Color c_my = _cb.color;
    Bitboard own_pieces = _cb.piece(c_my, PieceType::ALL);
    Bitboard all_pieces = _cb.All();

    const auto Add_Vaild_Dest_Sq = [&] (const auto &__f, Bitboard piece)
    {
        piece &= ~pinned_pieces;
        while (piece != 0)
        {
            Square __pos = next_idx(piece);
            add_move_to_list(__pos,
                __f(__pos, own_pieces, all_pieces) & valid_sq, _cb, myMoves);
        }
    };

    pawn_movement(_cb, myMoves, pinned_pieces, KA, _cb.legal_squares_mask);

    Add_Vaild_Dest_Sq(bishop_legal_squares, _cb.piece(c_my, BISHOP));
    Add_Vaild_Dest_Sq(knight_legal_squares, _cb.piece(c_my, KNIGHT));
    Add_Vaild_Dest_Sq(rook_legal_squares  , _cb.piece(c_my, ROOK  ));
    Add_Vaild_Dest_Sq(queen_legal_squares , _cb.piece(c_my, QUEEN ));
}

#endif

#ifndef KING_MOVE_GENERATION

static Bitboard
generate_AttackedSquares(const ChessBoard& _cb)
{
    Color c_my  =  _cb.color;
    Color c_emy = ~_cb.color;

    Bitboard ans = 0;
    Bitboard Apieces = _cb.All() ^ _cb.piece(c_my, KING);

    const auto attacked_squares = [&] (const auto &__f, Bitboard piece)
    {
        Bitboard res = 0;
        while (piece)
            ans |= __f(next_idx(piece), Apieces);
        return res;
    };

    ans |= pawn_atk_sq(_cb, c_emy);
    ans |= attacked_squares(bishop_atk_sq, _cb.piece(c_emy, BISHOP));
    ans |= attacked_squares(knight_atk_sq, _cb.piece(c_emy, KNIGHT));
    ans |= attacked_squares(rook_atk_sq  , _cb.piece(c_emy, ROOK  ));
    ans |= attacked_squares(queen_atk_sq , _cb.piece(c_emy, QUEEN ));
    ans |= plt::KingMasks[idx_no(_cb.piece(c_emy, KING))];

    return ans;
}

static void
king_attackers(ChessBoard &_cb)
{
    Bitboard attacked_sq = _cb.enemy_attacked_squares;

    Color c_my  =  _cb.color;
    Color c_emy = ~_cb.color;


    if (attacked_sq && ((_cb.piece(c_my, KING) & attacked_sq)) == 0)
    {
        _cb.KA = 0;
        return;
    }

    Square kpos = idx_no(_cb.piece(c_my, KING));
    Bitboard _Ap = _cb.All();
    int attk_count = 0;
    Bitboard attk_area = 0;

    const auto add_piece = [&] (const auto& __f, Bitboard enemy)
    {
        Bitboard area  = __f(kpos, _Ap);
        Bitboard piece = area & enemy;

        if (piece == 0) return;

        if ((piece & (piece - 1)) != 0)
            ++attk_count;

        ++attk_count;
        attk_area |= (area & __f(idx_no(piece), _Ap)) | piece;
    };

    add_piece(rook_atk_sq  , _cb.piece(c_emy, QUEEN) | _cb.piece(c_emy, ROOK  ));
    add_piece(bishop_atk_sq, _cb.piece(c_emy, QUEEN) | _cb.piece(c_emy, BISHOP));
    add_piece(knight_atk_sq, _cb.piece(c_emy, KNIGHT));

    Bitboard pawn_sq = plt::PawnCaptureMasks[c_my][kpos] & _cb.piece(c_emy, PAWN);
    if (pawn_sq != 0)
    {
        ++attk_count;
        attk_area |= pawn_sq;
    }

    _cb.KA = attk_count;
    _cb.legal_squares_mask = attk_area;
}

static void
KingMoves(const ChessBoard& _cb, MoveList& myMoves, Bitboard Attacked_Sq)
{
    const auto add_castle_move = [&] (Square ip, Square fp)
    {
        int pt = 6;
        int priority = 20 << 21;
        int color_bit = _cb.color << 20;
        Move enc_move = priority + color_bit + (pt << 12) + (fp << 6) + ip;
        myMoves.Add(enc_move);
    };

    Color c_my = _cb.color;

    Square kpos = idx_no(_cb.piece(c_my, KING));
    Bitboard K_sq = plt::KingMasks[kpos];
    Bitboard ans = K_sq & (~(_cb.piece(c_my, PieceType::ALL) | Attacked_Sq));

    add_move_to_list(kpos, ans, _cb, myMoves);

    if (!(_cb.csep & 1920) || ((1ULL << kpos) & Attacked_Sq)) return;

    Bitboard Apieces = _cb.All();
    Bitboard covered_squares = Apieces | Attacked_Sq;

    int shift         = 56 * (c_my ^ 1);
    Bitboard l_mid_sq = 2ULL << shift;
    Bitboard r_sq     = 96ULL << shift;
    Bitboard l_sq     = 12ULL << shift;

    int king_side  = 256 << (2 * c_my);
    int queen_side = 128 << (2 * c_my);

    // Can castle king_side  and no pieces are in-between
    if ((_cb.csep & king_side) and !(r_sq & covered_squares))
        add_castle_move(kpos, 6 + shift);

    // Can castle queen_side and no pieces are in-between
    if ((_cb.csep & queen_side) and !(Apieces & l_mid_sq) and !(l_sq & covered_squares))
        add_castle_move(kpos, 2 + shift);
}


#endif

#ifndef LEGAL_MOVES_CHECK

static Bitboard
legal_pinned_pieces(const ChessBoard& _cb)
{
    Color c_my  =  _cb.color;
    Color c_emy = ~_cb.color;

    Square kpos  = idx_no(_cb.piece(c_my, KING));
    Bitboard _Ap = _cb.All();

    Bitboard erq = _cb.piece(c_emy, QUEEN) | _cb.piece(c_emy, ROOK  );
    Bitboard ebq = _cb.piece(c_emy, QUEEN) | _cb.piece(c_emy, BISHOP);
    Bitboard  rq = _cb.piece(c_my , QUEEN) | _cb.piece(c_my , ROOK  );
    Bitboard  bq = _cb.piece(c_my , QUEEN) | _cb.piece(c_my , BISHOP);
    
    Bitboard pinned_pieces = 0;
    Bitboard ray_line = plt::LineMasks[kpos];
    Bitboard ray_diag = plt::DiagonalMasks[kpos];
    
    if (!((ray_line & erq) | (ray_diag & ebq)))
        return 0;

    const auto can_pinned = [&] (const auto &__f, const auto *table,
            Bitboard ownP, Bitboard emyP, const char pawn) -> bool
    {
        Bitboard pieces = table[kpos] & _Ap;
        Bitboard first_piece  = __f(pieces);
        Bitboard second_piece = __f(pieces ^ first_piece);

        Square index_f = idx_no(first_piece);
        Square index_s = idx_no(second_piece);

        if (  !(first_piece & _cb.piece(c_my, PieceType::ALL))
            or (!(second_piece & emyP))) return false;

        pinned_pieces |= first_piece;

        if (_cb.KA == 1) return false;

        if ((first_piece & ownP) != 0)
            return ((table[kpos] ^ table[index_s] ^ first_piece) != 0);


        Bitboard Rank63[2] = {Rank6, Rank3};

        Square eps    = _cb.csep & 127;
        const auto shift = (c_my == 1) ? (l_shift) : (r_shift);

        Bitboard n_pawn  = first_piece;
        Bitboard s_pawn  = (shift(n_pawn, 8) & (~_cb.All())) & Rank63[c_my];
        Bitboard free_sq = ~_cb.All();

        if (pawn == 's' && (first_piece & _cb.piece(c_my, PAWN)))
            return ((shift(n_pawn, 8) | shift(s_pawn, 8)) & free_sq) != 0;

        if (pawn == 'c' && (first_piece & _cb.piece(c_my, PAWN)))
        {
            Bitboard capt_sq = plt::PawnCaptureMasks[c_my][index_f];
            Bitboard dest_sq = capt_sq & second_piece;

            if (eps != 64 && (table[kpos] & capt_sq & (1ULL << eps)) != 0)
                dest_sq |= 1ULL << eps;

            return dest_sq;
        }

        return false;
    };

    if (   can_pinned(lSb, plt::RightMasks,  rq, erq, '-')
        or can_pinned(mSb, plt::LeftMasks ,  rq, erq, '-')
        or can_pinned(lSb, plt::UpMasks   ,  rq, erq, 's')
        or can_pinned(mSb, plt::DownMasks ,  rq, erq, 's')) return 1;

    if (   can_pinned(lSb, plt::UpRightMasks  , bq, ebq, 'c')
        or can_pinned(lSb, plt::UpLeftMasks   , bq, ebq, 'c')
        or can_pinned(mSb, plt::DownRightMasks, bq, ebq, 'c')
        or can_pinned(mSb, plt::DownLeftMasks , bq, ebq, 'c')) return 1;

    return pinned_pieces;
}

static bool
legal_pawns_move(const ChessBoard& _cb, Bitboard pinned_pieces, Bitboard atk_area)
{
    Bitboard Rank27[2] = {Rank2, Rank7};
    Bitboard Rank63[2] = {Rank6, Rank3};
    const auto shift = (_cb.color == 1) ? (l_shift) : (r_shift);

    Color c_my  =  _cb.color;
    Color c_emy = ~_cb.color;

    int own = int(c_my ) << 3;
    int emy = int(c_emy) << 3;

    const auto legal_enpassant_pawns = [&] (Bitboard l_pawns, Bitboard r_pawns, Square ep)
    {
        Square kpos = idx_no(_cb.piece(c_my, KING));
        Bitboard ep_square = 1ULL << ep;

        if ((ep == 64) or (_cb.KA == 1 and !(plt::PawnCaptureMasks[c_my][kpos] & _cb.piece(c_emy, PAWN))))
            return false;

        return ((shift(l_pawns, 7 + (own >> 2)) & ep_square) and en_passant_recheck(ep + (2 * emy - 9), _cb))
            or ((shift(r_pawns, 7 + (emy >> 2)) & ep_square) and en_passant_recheck(ep + (2 * emy - 7), _cb));
    };

    const auto legal_promotion_pawns = [&] (Bitboard pawns, Bitboard move_sq, Bitboard capt_sq)
    {
        Bitboard valid_squares = 0;
        while (pawns != 0)
        {
            Square __pos = next_idx(pawns);
            valid_squares |= (plt::PawnMasks[c_my][__pos] & move_sq)
                           | (plt::PawnCaptureMasks[c_my][__pos] & capt_sq);
        }

        return (valid_squares != 0);
    };

  
    Bitboard pawns   = _cb.piece(c_my, PAWN) ^ (_cb.piece(c_my, PAWN) & pinned_pieces);
    Bitboard e_pawns = pawns & Rank27[c_my];
    Bitboard n_pawns = pawns ^ e_pawns;
    Bitboard l_pawns = n_pawns & RightAttkingPawns;
    Bitboard r_pawns = n_pawns &  LeftAttkingPawns;
    Bitboard s_pawns = (shift(n_pawns, 8) & (~_cb.All())) & Rank63[c_my];

    Bitboard free_sq = ~_cb.All();
    Bitboard enemyP  = _cb.piece(c_emy, PieceType::ALL);
    Bitboard capt_sq = (atk_area &  enemyP) * (_cb.KA) + ( enemyP) * (1 - _cb.KA);
    Bitboard move_sq = (atk_area ^ capt_sq) * (_cb.KA) + (free_sq) * (1 - _cb.KA);


    if (legal_enpassant_pawns(l_pawns, r_pawns, _cb.csep & 127))
        return true;
    
    if (legal_promotion_pawns(e_pawns, move_sq, capt_sq))
        return true;

    Bitboard valid_squares = 0;
    
    // Capture Squares
    valid_squares |= shift(l_pawns, 7 + (own >> 2)) & capt_sq;
    valid_squares |= shift(r_pawns, 7 + (emy >> 2)) & capt_sq;

    // Non-Captures Squares
    valid_squares |= shift(s_pawns, 8) & move_sq;
    valid_squares |= shift(n_pawns, 8) & move_sq;

    return (valid_squares != 0);
}

static bool
legal_piece_move(const ChessBoard &_cb)
{
    const auto pinned_pieces = legal_pinned_pieces(_cb);
    const auto filter = _cb.KA * _cb.legal_squares_mask + (1 - _cb.KA) * AllSquares;

    if (pinned_pieces & 1)
        return true;

    Color c_my = _cb.color;

    Bitboard  my_pieces = _cb.piece(c_my, PieceType::ALL);
    Bitboard all_pieces = _cb.All();

    const auto canMove = [&] (const auto &__f, Bitboard piece)
    {
        piece &= ~pinned_pieces;
        Bitboard legal_squares = 0;

        while (piece != 0)
            legal_squares |= __f(next_idx(piece), my_pieces, all_pieces) & filter;

        return (legal_squares != 0);
    };

    return legal_pawns_move(_cb, pinned_pieces, _cb.legal_squares_mask)
        or canMove(knight_legal_squares, _cb.piece(c_my, KNIGHT))
        or canMove(bishop_legal_squares, _cb.piece(c_my, BISHOP))
        or canMove(  rook_legal_squares, _cb.piece(c_my, ROOK  ))
        or canMove( queen_legal_squares, _cb.piece(c_my, QUEEN ));
}

static bool
legal_king_move(const ChessBoard& _cb, Bitboard attacked_squares)
{
    Color c_my = _cb.color;
    Square kpos = idx_no(_cb.piece(c_my, KING));
    Bitboard K_sq = plt::KingMasks[kpos];

    Bitboard legal_squares = K_sq & (~(_cb.piece(c_my, PieceType::ALL) | attacked_squares));

    // If no castling is possible or king is attacked by a enemy piece
    if (!(_cb.csep & 1920) or ((1ULL << kpos) & attacked_squares))
        return (legal_squares != 0);

    Bitboard Apieces = _cb.All();
    Bitboard covered_squares = Apieces | attacked_squares;

    int shift         = 56 * (_cb.color ^ 1);
    Bitboard l_mid_sq = 2ULL << shift;
    Bitboard r_sq     = 96ULL << shift;
    Bitboard l_sq     = 12ULL << shift;

    Bitboard king_side  = 256 << (2 * _cb.color);
    Bitboard queen_side = 128 << (2 * _cb.color);
    
    // Legal moves possible if king can castle (king or queen) side
    // and no squares between king and rook is attacked by enemy piece
    // or occupied by own piece. 
    return ((_cb.csep &  king_side) and !(r_sq & covered_squares))
        or ((_cb.csep & queen_side) and !(Apieces & l_mid_sq) and !(l_sq & covered_squares));
}

#endif

#ifndef SEARCH_HELPER

bool
is_passad_pawn(Square idx, ChessBoard& _cb)
{
    if (_cb.color == Color::WHITE)
    {
        Bitboard res = plt::UpMasks[idx];
        if ((idx & 7) >= 1) res |= plt::UpMasks[idx - 1];
        if ((idx & 7) <= 6) res |= plt::UpMasks[idx + 1];
        if (res & _cb.piece(Color::BLACK, PAWN)) return false;
        return true;
    }

    Bitboard res = plt::DownMasks[idx];
    if ((idx & 7) >= 1) res |= plt::DownMasks[idx - 1];
    if ((idx & 7) <= 6) res |= plt::DownMasks[idx + 1];
    if (res & _cb.piece(Color::WHITE, PAWN)) return false;
    return true;
}

bool
interesting_move(Move move, ChessBoard& _cb)
{
    Square ip = move & 63;
    Square fp = (move >> 6) & 63;
    Square ep = _cb.csep & 127;

    PieceType it = PieceType((move >> 12) & 7);

    Piece fpt = _cb.piece_on_square(fp);

    // If it captures a piece.
    if (fpt != Piece::NO_PIECE)
        return true;
    
    // An enpassant-move or a passed pawn
    if (it == PieceType::PAWN) {
        if (fp == ep) return true;
        if (is_passad_pawn(ip, _cb)) return true;
    }

    // If its castling
    if ((it == PieceType::KING) && std::abs(fp - ip) == 2) return true;

    // If the move gives a check.
    _cb.MakeMove(move);
    bool res = in_check(_cb);
    _cb.UnmakeMove();
    return res;

    // return false;
}

bool
f_prune_move(Move move, ChessBoard& _cb)
{
    // Square ip = move & 63;
    Square fp = (move >> 6) & 63;

    PieceType it = PieceType((move >> 12) & 7);
    PieceType ft = PieceType((move >> 15) & 7);

    if (ft != PieceType::NONE) return true;
    if (it == PieceType::PAWN)
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


int
search_extension(const ChessBoard& pos, int numExtensions)
{

    if (numExtensions >= EXTENSION_LIMIT)
        return 0;

    if (pos.KA > 1)
        return 1;

    return 0;
}

#endif

#ifndef GENERATOR


bool
has_legal_moves(ChessBoard& _cb)
{
    _cb.remove_movegen_extra_data();

    king_attackers(_cb);
    if ((_cb.KA < 2) and legal_piece_move(_cb))
        return true;

    _cb.enemy_attacked_squares = generate_AttackedSquares(_cb);
    return legal_king_move(_cb, _cb.enemy_attacked_squares);
}

MoveList
generate_moves(ChessBoard& _cb, bool qs_only)
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
        _cb.enemy_attacked_squares = generate_AttackedSquares(_cb);

    if (_cb.attackers_found() == false)
        king_attackers(_cb);

    if (_cb.KA < 2)
        piece_movement(_cb, myMoves, _cb.KA);
    
    KingMoves(_cb, myMoves, _cb.enemy_attacked_squares );

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

