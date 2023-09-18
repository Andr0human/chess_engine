
#include "movegen.h"


#ifndef MOVEGEN_UTILS


/*
    (priority) (check) (flag) (color) (promotion) (piece_fp) (piece_ip)   (fp)    (ip)
      000000      0      00      0        00         000        000      000000  000000

    0 -> Quiet
    1 -> Castling
    2 -> Captures
    3 -> Promotion (captures included)
    4 -> Checks
*/


template <Color c_my>
Bitboard
PawnAttackSquares(const ChessBoard& _cb)
{
    constexpr int inc  = 2 * int(c_my) - 1;
    Bitboard pawns = _cb.piece<c_my, PAWN>();
    const auto shifter = (c_my == WHITE) ? LeftShift : RightShift;

    return shifter(pawns & RightAttkingPawns, 8 + inc) |
           shifter(pawns & LeftAttkingPawns , 8 - inc);
}

template <Color c_my>
bool
MoveGivesCheck(Move move, ChessBoard& pos, const MoveList& myMoves)
{
    Square ip = Square(move & 63);
    Square fp = Square((move >> 6) & 63);

    PieceType pt = PieceType((move >> 12) & 7);

    // Direct Checks
    if ((pt != KING) and (((1ULL << fp) & myMoves.squaresThatCheckEnemyKing[pt - 1]) != 0))
        return true;

    // Discover Checks
    if (((1ULL << ip) & myMoves.discoverCheckSquares) == 0)
        return false;

    // Last Resort
    pos.MakeMove(move);
    bool gives_check = InCheck< ~c_my>(pos);
    pos.UnmakeMove();
    return gives_check;
}


template <Color c_my>
static bool
EnpassantRecheck(Square ip, const ChessBoard& _cb)
{
    constexpr Color c_emy = ~c_my;

    Square kpos = SquareNo( _cb.piece<c_my, KING>() );
    Square eps = Square(_cb.csep & 127);

    Bitboard erq = _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, ROOK>();
    Bitboard ebq = _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, BISHOP>();
    Bitboard  Ap = _cb.All() ^ ((1ULL << ip) | (1ULL << (eps - 8 * (2 * c_my - 1))));

    Bitboard res  = Msb((plt::LeftMasks[kpos] & Ap)) | Lsb((plt::RightMasks[kpos] & Ap));
    Bitboard tmp1 = Msb((plt::DownLeftMasks[kpos] & Ap)) | Lsb((plt::UpRightMasks[kpos] & Ap));
    Bitboard tmp2 = Lsb((plt::UpLeftMasks[kpos] & Ap)) | Msb((plt::DownRightMasks[kpos] & Ap));

    if (res & erq) return false;
    // Board has to be invalid for this check
    if ((tmp1 | tmp2) & ebq) return false;

    return true;
}


bool
IsLegalMoveForPosition(Move move, ChessBoard& pos)
{
    Square ip = Square(move & 63);
    Square fp = Square((move >> 6) & 63);

    Piece ipt = pos.PieceOnSquare(ip);
    Piece fpt = pos.PieceOnSquare(fp);

    Color side = pos.color;

    // If no piece on init_sq or piece_color != side_to_move
    if (((ipt & 7) == 0) or (((ipt >> 3) & 1) != side))
        return false;

    // If capt_piece_color == side_to_move
    if ((fpt != 0) and ((fpt >> 3) & 1) == side)
        return false;

    MoveList myMoves = GenerateMoves(pos);

    for (Move legal_move : myMoves.pMoves)
        if (filter(move) == filter(legal_move)) return true;
    
    return false;
}

template <Color color>
static void
AddPawnMoves(Square ip, Square fp, const ChessBoard& pos,
    MoveList& myMoves, int flag, int priority)
{
    if ((myMoves.qsSearch) and !(flag == CAPTURES or flag == PROMOTION))
        return;
    
    PieceType ipt = type_of(pos.PieceOnSquare(ip));
    PieceType fpt = type_of(pos.PieceOnSquare(fp));

    constexpr int color_bit = color << 20;

    Move move =
        (priority << 24) | (flag << 21) | color_bit |
        (fpt      << 15) | (ipt  << 12) | (fp << 6) | ip;

    myMoves.Add(move);

    if (flag == PROMOTION)
    {
        // Add promotion bit
        myMoves.Add(move | 0xC0000);
        myMoves.Add(move | 0x80000);
        myMoves.Add(move | 0x40000);
    }
}

template<Color c_my>
static void
AddShiftPawnMoves(Bitboard endSquares, int shift, const ChessBoard& pos,
    MoveList& myMoves, int flag, int start_priority)
{
    if (endSquares == 0)
        return;

    constexpr Color c_emy = ~c_my;
    Bitboard emy_pieces = pos.piece<c_emy, ALL>();
    constexpr int color_bit = c_my << 20;
    constexpr PieceType ipt = PAWN;

    if (myMoves.qsSearch)
        endSquares &= emy_pieces;

    Move base_move = color_bit | (ipt << 12);

    while (endSquares != 0)
    {
        Square fp = NextSquare(endSquares);
        Square ip = fp + shift;

        PieceType fpt = type_of(pos.PieceOnSquare(fp));

        int priority = start_priority;

        if (flag == CAPTURES)
            priority += 2 * (fpt - ipt) + 15;

        Move move = base_move | (priority << 24) | (flag << 21)
                  | (fpt << 15) | (fp << 6) | ip;

        myMoves.Add(move);
    }
}


template<Color c_my>
static void
AddCaptureMoves(Square ip, Bitboard endSquares, const ChessBoard& pos, MoveList& myMoves, int start_priority)
{
    constexpr Color c_emy = ~c_my;
    constexpr int flag_bit  = CAPTURES << 21;
    constexpr int color_bit = c_my     << 20;


    Bitboard emy_pieces = pos.piece<c_emy, ALL>();
    PieceType ipt = type_of(pos.PieceOnSquare(ip));

    if (myMoves.qsSearch)
        endSquares &= emy_pieces;

    Move base_move = color_bit | (ipt << 12) | ip;

    while (endSquares != 0)
    {
        Square fp = NextSquare(endSquares);
        PieceType fpt = type_of(pos.PieceOnSquare(fp));
        int priority = start_priority;

        priority += 2 * (fpt - ipt) + 15;

        // Generate Move Priority

        // if (((1ULL << ip) & pos.enemyAttackedSquares) != 0)
        //     priority += 2;

        // if (((1ULL << fp) & pos.enemyAttackedSquares) != 0)
        //     priority -= 4;

        Move move = base_move | (priority << 24) | flag_bit | (fpt << 15) | (fp << 6);
        myMoves.Add(move);
    }
}

template<Color c_my>
static void
AddQuietMoves(Square ip, Bitboard endSquares, const ChessBoard& pos, MoveList& myMoves, int flag, int priority)
{
    constexpr int color_bit = c_my     << 20;
    const int flag_bit      = flag     << 21;
    const int priority_bit  = priority << 24;

    if (myMoves.qsSearch)
        return;
    
    PieceType ipt  = type_of(pos.PieceOnSquare(ip));
    Move base_move = color_bit | (ipt << 12) | ip;

    while (endSquares != 0)
    {
        Square fp = NextSquare(endSquares);
        Move move = base_move | priority_bit | flag_bit | (fp << 6);
        myMoves.Add(move);
    }
}


#endif

#ifndef Piece_Movement

#ifndef PAWNS

template <Color c_my>
static inline void
EnpassantPawns(const ChessBoard &_cb, MoveList &myMoves,
    Bitboard l_pawns, Bitboard r_pawns, int KA)
{
    constexpr Color c_emy = ~c_my;
    constexpr int own =  c_my << 3;
    constexpr int emy = c_emy << 3;

    Square kpos = SquareNo( _cb.piece<c_my, KING>() );
    Square eps = Square(_cb.csep & 127);
    Bitboard _ep = 1ULL << eps;

    if (eps == 64 || (KA == 1 and !(plt::PawnCaptureMasks[c_my][kpos] & _cb.piece<c_emy, PAWN>())))
        return;

    const auto shift = c_my == Color::WHITE ? LeftShift : RightShift;

    if ((shift(l_pawns, 7 + (own >> 2)) & _ep) and EnpassantRecheck<c_my>(eps + (2 * emy - 9), _cb))
        AddPawnMoves<c_my>(eps + (2 * emy - 9), eps, _cb, myMoves, CAPTURES, 32);

    if ((shift(r_pawns, 7 + (emy >> 2)) & _ep) and EnpassantRecheck<c_my>(eps + (2 * emy - 7), _cb))
        AddPawnMoves<c_my>(eps + (2 * emy - 7), eps, _cb, myMoves, CAPTURES, 32);
}

template <Color c_my>
static inline void
PromotionPawns(const ChessBoard& pos, MoveList &myMoves,
    Bitboard move_sq, Bitboard capt_sq, Bitboard pawns)
{
    while (pawns != 0)
    {
        Square ip = NextSquare(pawns);
        Bitboard endSquares = (plt::PawnMasks[c_my][ip] & move_sq)
                     | (plt::PawnCaptureMasks[c_my][ip] & capt_sq);

        while (endSquares != 0)
        {
            Square fp = NextSquare(endSquares);
            PieceType fpt = type_of(pos.PieceOnSquare(fp));
            AddPawnMoves<c_my>(ip, fp, pos, myMoves, PROMOTION, 2 * fpt + 30);
        }
    }
}


template <Color c_my>
static inline void
PawnMovement(const ChessBoard &_cb, MoveList &myMoves,
    Bitboard pin_pieces, int KA, Bitboard atk_area)
{
    constexpr Bitboard Rank27[2] = {Rank2, Rank7};
    constexpr Bitboard Rank63[2] = {Rank6, Rank3};

    constexpr Color c_emy = ~c_my;
    constexpr int own = int(c_my) << 3;
    constexpr int emy = int(c_emy) << 3;

    const auto shift = (c_my == WHITE) ? (LeftShift) : (RightShift);

    Bitboard pawns   = _cb.piece<c_my, PAWN>() & (~pin_pieces);
    Bitboard e_pawns = pawns & Rank27[c_my];
    Bitboard n_pawns = pawns ^ e_pawns;
    Bitboard l_pawns = n_pawns & RightAttkingPawns;
    Bitboard r_pawns = n_pawns &  LeftAttkingPawns;
    Bitboard s_pawns = (shift(n_pawns, 8) & (~_cb.All())) & Rank63[c_my];

    Bitboard free_sq = ~_cb.All();
    Bitboard enemyP  = _cb.piece<c_emy, ALL>();
    Bitboard capt_sq = (atk_area &  enemyP) * (KA) + ( enemyP) * (1 - KA);
    Bitboard move_sq = (atk_area ^ capt_sq) * (KA) + (free_sq) * (1 - KA);

    Bitboard l_captures = shift(l_pawns, 7 + (own >> 2)) & capt_sq;
    Bitboard r_captures = shift(r_pawns, 7 + (emy >> 2)) & capt_sq;

    EnpassantPawns<c_my>(_cb, myMoves, l_pawns, r_pawns, KA);
    PromotionPawns<c_my>(_cb, myMoves, move_sq, capt_sq, e_pawns);

    AddShiftPawnMoves<c_my>(l_captures, 2 * emy - 9, _cb, myMoves, CAPTURES, 10);
    AddShiftPawnMoves<c_my>(r_captures, 2 * emy - 7, _cb, myMoves, CAPTURES, 10);

    AddShiftPawnMoves<c_my>(shift(s_pawns, 8) & move_sq, 4 * emy - 16, _cb, myMoves, NORMAL, 6);
    AddShiftPawnMoves<c_my>(shift(n_pawns, 8) & move_sq, 2 * emy -  8, _cb, myMoves, NORMAL, 6);
}

#endif

template <Color c_my>
static Bitboard
PinnedPiecesList(const ChessBoard& _cb, MoveList &myMoves, int KA)
{
    constexpr Color c_emy = ~c_my;

    Square kpos = SquareNo( _cb.piece<c_my, KING>() );
    Bitboard _Ap = _cb.All();

    Bitboard erq = _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, ROOK>();
    Bitboard ebq = _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, BISHOP>();
    Bitboard  rq = _cb.piece<c_my , QUEEN>() | _cb.piece<c_my, ROOK>();
    Bitboard  bq = _cb.piece<c_my , QUEEN>() | _cb.piece<c_my, BISHOP>();

    Bitboard pinned_pieces = 0;

    if ((    plt::LineMasks[kpos] & erq) == 0 and
        (plt::DiagonalMasks[kpos] & ebq) == 0) return 0ULL;
    

    const auto pins_check = [&] (const auto &__f, Bitboard *table,
        Bitboard sliding_piece, Bitboard emy_sliding_piece, char pawn)
    {
        Bitboard pieces = table[kpos] & _Ap;
        Bitboard first_piece  = __f(pieces);
        Bitboard second_piece = __f(pieces ^ first_piece);

        Square index_f = SquareNo(first_piece);
        Square index_s = SquareNo(second_piece);

        if (   !(first_piece  & _cb.piece<c_my, ALL>())
            or !(second_piece & emy_sliding_piece)) return;

        pinned_pieces |= first_piece;

        if (KA == 1) return;

        if ((first_piece & sliding_piece) != 0)
        {
            Bitboard emy_pieces = _cb.piece<c_emy, ALL>();
            Bitboard dest_sq = table[kpos] ^ table[index_s] ^ first_piece;
            Bitboard capt_sq  = dest_sq & emy_pieces;
            Bitboard quiet_sq = dest_sq ^ capt_sq;

            AddCaptureMoves<c_my>(index_f, capt_sq, _cb, myMoves, 10);
            AddQuietMoves<c_my>(index_f, quiet_sq, _cb, myMoves, NORMAL, 6);

            return;
        }

        constexpr Bitboard Rank63[2] = {Rank6, Rank3};

        Square eps = Square(_cb.csep & 127);
        const auto shift = (c_my == WHITE) ? (LeftShift) : (RightShift);

        Bitboard n_pawn  = first_piece;
        Bitboard free_sq = ~_cb.All();
        Bitboard s_pawn  = shift(n_pawn, 8) & free_sq;
        Bitboard ss_pawn = shift(s_pawn & Rank63[c_my], 8) & free_sq;

        if ((pawn == 's') and (first_piece & _cb.piece<c_my, PAWN>()))
        {
            if (s_pawn != 0)
                AddPawnMoves<c_my>(index_f, SquareNo(s_pawn),  _cb, myMoves, NORMAL, 6);
            
            if (ss_pawn != 0)
                AddPawnMoves<c_my>(index_f, SquareNo(ss_pawn), _cb, myMoves, NORMAL, 6);
        }

        if ((pawn == 'c') and (first_piece & _cb.piece<c_my, PAWN>()))
        {
            Bitboard capt_sq = plt::PawnCaptureMasks[c_my][index_f];
            Bitboard dest_sq = capt_sq & second_piece;

            if ((eps != 64) and (table[kpos] & capt_sq & (1ULL << eps)) != 0)
                dest_sq |= 1ULL << eps;

            int flag = (dest_sq & Rank18) != 0 ? PROMOTION : CAPTURES;

            if (dest_sq != 0)
                AddPawnMoves<c_my>(index_f, SquareNo(dest_sq), _cb, myMoves, flag, 28);
        }
    };


    pins_check(Lsb, plt::RightMasks, rq, erq, '-');
    pins_check(Msb, plt::LeftMasks, rq, erq, '-');
    
    pins_check(Lsb, plt::UpMasks, rq, erq, 's');
    pins_check(Msb, plt::DownMasks, rq, erq, 's');
    
    pins_check(Lsb, plt::UpRightMasks, bq, ebq, 'c');
    pins_check(Lsb, plt::UpLeftMasks, bq, ebq, 'c');
    
    pins_check(Msb, plt::DownRightMasks, bq, ebq, 'c');
    pins_check(Msb, plt::DownLeftMasks, bq, ebq, 'c');

    return pinned_pieces;
} 


static inline Bitboard
KnightLegalSquares(Square __pos, Bitboard _Op, Bitboard _Ap)
{ return ~_Op & KnightAttackSquares(__pos, _Ap); }

static inline Bitboard
BishopLegalSquares(Square __pos, Bitboard _Op, Bitboard _Ap)
{ return ~_Op & BishopAttackSquares(__pos, _Ap); }

static inline Bitboard
RookLegalSquares(Square __pos, Bitboard _Op, Bitboard _Ap)
{ return ~_Op & RookAttackSquares(__pos, _Ap); }

static inline Bitboard
QueenLegalSquares(Square __pos, Bitboard _Op, Bitboard _Ap)
{ return ~_Op & QueenAttackSquares(__pos, _Ap); }


template <Color c_my>
static void
PieceMovement(ChessBoard& _cb, MoveList& myMoves, int KA)
{
    constexpr Color c_emy = ~c_my;
    myMoves.pinnedPiecesSquares  = PinnedPiecesList<c_my>(_cb, myMoves, _cb.checkers);
    Bitboard pinned_pieces = myMoves.pinnedPiecesSquares;
    Bitboard valid_squares = KA * _cb.legalSquaresMaskInCheck + (1 - KA) * AllSquares;

    Bitboard own_pieces = _cb.piece<c_my , ALL>();
    Bitboard emy_pieces = _cb.piece<c_emy, ALL>();
    Bitboard occupied = _cb.All();

    const auto AddLegalSquares = [&] (const auto& GenSquares, Bitboard piece)
    {
        piece &= ~pinned_pieces;
        while (piece != 0)
        {
            Square ip = NextSquare(piece);
            Bitboard dest_squares  = GenSquares(ip, own_pieces, occupied) & valid_squares;
            Bitboard capt_squares  = dest_squares & emy_pieces;
            Bitboard quiet_squares = dest_squares ^ capt_squares;

            AddCaptureMoves<c_my>(ip, capt_squares, _cb, myMoves, 10);
            AddQuietMoves<c_my>(ip, quiet_squares, _cb, myMoves, NORMAL, 6);
        }
    };

    PawnMovement<c_my>(_cb, myMoves, pinned_pieces, KA, _cb.legalSquaresMaskInCheck);

    AddLegalSquares(BishopLegalSquares, _cb.piece<c_my, BISHOP>());
    AddLegalSquares(KnightLegalSquares, _cb.piece<c_my, KNIGHT>());
    AddLegalSquares(RookLegalSquares  , _cb.piece<c_my, ROOK  >() );
    AddLegalSquares(QueenLegalSquares , _cb.piece<c_my, QUEEN >() );
}

#endif

#ifndef KING_MOVE_GENERATION

template <Color c_my>
static Bitboard
GenerateAttackedSquares(const ChessBoard& _cb)
{
    constexpr Color c_emy = ~c_my;

    Bitboard ans = 0;
    Bitboard Apieces = _cb.All() ^ _cb.piece<c_my, KING>();

    const auto attacked_squares = [&] (const auto &__f, Bitboard piece)
    {
        Bitboard res = 0;
        while (piece != 0)
            ans |= __f(NextSquare(piece), Apieces);
        return res;
    };

    ans |= PawnAttackSquares<c_emy>(_cb);
    ans |= attacked_squares(BishopAttackSquares, _cb.piece<c_emy, BISHOP>());
    ans |= attacked_squares(KnightAttackSquares, _cb.piece<c_emy, KNIGHT>());
    ans |= attacked_squares(RookAttackSquares  , _cb.piece<c_emy, ROOK  >());
    ans |= attacked_squares(QueenAttackSquares , _cb.piece<c_emy, QUEEN >());
    ans |= plt::KingMasks[SquareNo(_cb.piece<c_emy, KING>())];

    return ans;
}

template <Color c_my>
static void
KingAttackers(ChessBoard& _cb)
{
    Bitboard attacked_sq = _cb.enemyAttackedSquares;
    constexpr Color c_emy = ~c_my;

    if (attacked_sq && ((_cb.piece<c_my, KING>() & attacked_sq)) == 0)
    {
        _cb.checkers = 0;
        return;
    }

    Square kpos = SquareNo(_cb.piece<c_my, KING>());
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
        attk_area |= (area & __f(SquareNo(piece), _Ap)) | piece;
    };

    add_piece(RookAttackSquares  , _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, ROOK>());
    add_piece(BishopAttackSquares, _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, BISHOP>());
    add_piece(KnightAttackSquares, _cb.piece<c_emy, KNIGHT>());

    Bitboard pawn_sq = plt::PawnCaptureMasks[c_my][kpos] & _cb.piece<c_emy, PAWN>();
    if (pawn_sq != 0)
    {
        ++attk_count;
        attk_area |= pawn_sq;
    }

    _cb.checkers = attk_count;
    _cb.legalSquaresMaskInCheck = attk_area;
}

template <Color c_my>
static void
KingMoves(const ChessBoard& _cb, MoveList& myMoves, Bitboard attackedSq)
{
    constexpr Color c_emy = ~c_my;
    Square kpos = SquareNo(_cb.piece<c_my, KING>());
    Bitboard king_mask = plt::KingMasks[kpos];
    Bitboard emy_pieces = _cb.piece<c_emy, ALL>();

    Bitboard dest_sq  = king_mask & (~(_cb.piece<c_my, ALL>() | attackedSq));
    Bitboard capt_sq  = dest_sq & emy_pieces;
    Bitboard quiet_sq = dest_sq ^ capt_sq;

    // AddMoves(kpos, dest_sq, _cb, myMoves);
    AddCaptureMoves<c_my>(kpos, capt_sq, _cb, myMoves, 10);
    AddQuietMoves<c_my>(kpos, quiet_sq, _cb, myMoves, NORMAL, 6);

    if (!(_cb.csep & 1920) or ((1ULL << kpos) & attackedSq)) return;

    Bitboard Apieces = _cb.All();
    Bitboard covered_squares = Apieces | attackedSq;

    constexpr int      shift    = 56 * c_emy;
    constexpr Bitboard l_mid_sq = 2ULL << shift;
    constexpr Bitboard r_sq     = 96ULL << shift;
    constexpr Bitboard l_sq     = 12ULL << shift;

    constexpr int king_side  = 256 << (2 * c_my);
    constexpr int queen_side = 128 << (2 * c_my);

    // Can castle king_side  and no pieces are in-between
    if ((_cb.csep & king_side) and !(r_sq & covered_squares)) 
        AddQuietMoves<c_my>(kpos, 1ULL << (shift + 6), _cb, myMoves, CASTLING, 12);

    // Can castle queen_side and no pieces are in-between
    if ((_cb.csep & queen_side) and !(Apieces & l_mid_sq) and !(l_sq & covered_squares))
        AddQuietMoves<c_my>(kpos, 1ULL << (shift + 2), _cb, myMoves, CASTLING, 12);
}


#endif

#ifndef LEGAL_MOVES_CHECK 

template <Color c_my>
static Bitboard
LegalPinnedPieces(const ChessBoard& _cb)
{
    constexpr Color c_emy = ~c_my;

    Square kpos  = SquareNo(_cb.piece<c_my, KING>());
    Bitboard _Ap = _cb.All();

    Bitboard erq = _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, ROOK  >();
    Bitboard ebq = _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, BISHOP>();
    Bitboard  rq = _cb.piece<c_my , QUEEN>() | _cb.piece<c_my , ROOK  >();
    Bitboard  bq = _cb.piece<c_my , QUEEN>() | _cb.piece<c_my , BISHOP>();
    
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

        Square index_f = SquareNo(first_piece);
        Square index_s = SquareNo(second_piece);

        if (  !(first_piece & _cb.piece<c_my, ALL>())
            or (!(second_piece & emyP))) return false;

        pinned_pieces |= first_piece;

        if (_cb.checkers == 1) return false;

        if ((first_piece & ownP) != 0)
            return ((table[kpos] ^ table[index_s] ^ first_piece) != 0);


        Bitboard Rank63[2] = {Rank6, Rank3};

        Square eps    = Square(_cb.csep & 127);
        const auto shift = (c_my == 1) ? (LeftShift) : (RightShift);

        Bitboard n_pawn  = first_piece;
        Bitboard s_pawn  = (shift(n_pawn, 8) & (~_cb.All())) & Rank63[c_my];
        Bitboard free_sq = ~_cb.All();

        if (pawn == 's' && (first_piece & _cb.piece<c_my, PAWN>()))
            return ((shift(n_pawn, 8) | shift(s_pawn, 8)) & free_sq) != 0;

        if (pawn == 'c' && (first_piece & _cb.piece<c_my, PAWN>()))
        {
            Bitboard capt_sq = plt::PawnCaptureMasks[c_my][index_f];
            Bitboard dest_sq = capt_sq & second_piece;

            if (eps != 64 && (table[kpos] & capt_sq & (1ULL << eps)) != 0)
                dest_sq |= 1ULL << eps;

            return dest_sq;
        }

        return false;
    };

    if (   can_pinned(Lsb, plt::RightMasks,  rq, erq, '-')
        or can_pinned(Msb, plt::LeftMasks ,  rq, erq, '-')
        or can_pinned(Lsb, plt::UpMasks   ,  rq, erq, 's')
        or can_pinned(Msb, plt::DownMasks ,  rq, erq, 's')) return 1;

    if (   can_pinned(Lsb, plt::UpRightMasks  , bq, ebq, 'c')
        or can_pinned(Lsb, plt::UpLeftMasks   , bq, ebq, 'c')
        or can_pinned(Msb, plt::DownRightMasks, bq, ebq, 'c')
        or can_pinned(Msb, plt::DownLeftMasks , bq, ebq, 'c')) return 1;

    return pinned_pieces;
}

template <Color c_my>
static bool
LegalPawnMoves(const ChessBoard& _cb, Bitboard pinned_pieces, Bitboard atk_area)
{
    Bitboard Rank27[2] = {Rank2, Rank7};
    Bitboard Rank63[2] = {Rank6, Rank3};
    const auto shift = (_cb.color == 1) ? (LeftShift) : (RightShift);

    constexpr Color c_emy = ~c_my;
    constexpr int own = int(c_my ) << 3;
    constexpr int emy = int(c_emy) << 3;

    const auto legal_EnpassantPawns = [&] (Bitboard l_pawns, Bitboard r_pawns, Square ep)
    {
        Square kpos = SquareNo(_cb.piece<c_my, KING>());
        Bitboard ep_square = 1ULL << ep;

        if ((ep == 64) or (_cb.checkers == 1 and !(plt::PawnCaptureMasks[c_my][kpos] & _cb.piece<c_emy, PAWN>())))
            return false;

        return ((shift(l_pawns, 7 + (own >> 2)) & ep_square) and EnpassantRecheck<c_my>(ep + (2 * emy - 9), _cb))
            or ((shift(r_pawns, 7 + (emy >> 2)) & ep_square) and EnpassantRecheck<c_my>(ep + (2 * emy - 7), _cb));
    };

    const auto legal_PromotionPawns = [&] (Bitboard pawns, Bitboard move_sq, Bitboard capt_sq)
    {
        Bitboard valid_squares = 0;
        while (pawns != 0)
        {
            Square __pos = NextSquare(pawns);
            valid_squares |= (plt::PawnMasks[c_my][__pos] & move_sq)
                           | (plt::PawnCaptureMasks[c_my][__pos] & capt_sq);
        }

        return (valid_squares != 0);
    };

  
    Bitboard pawns   = _cb.piece<c_my, PAWN>() ^ (_cb.piece<c_my, PAWN>() & pinned_pieces);
    Bitboard e_pawns = pawns & Rank27[c_my];
    Bitboard n_pawns = pawns ^ e_pawns;
    Bitboard l_pawns = n_pawns & RightAttkingPawns;
    Bitboard r_pawns = n_pawns &  LeftAttkingPawns;
    Bitboard s_pawns = (shift(n_pawns, 8) & (~_cb.All())) & Rank63[c_my];

    Bitboard free_sq = ~_cb.All();
    Bitboard enemyP  = _cb.piece<c_emy, ALL>();
    Bitboard capt_sq = (atk_area &  enemyP) * (_cb.checkers) + ( enemyP) * (1 - _cb.checkers);
    Bitboard move_sq = (atk_area ^ capt_sq) * (_cb.checkers) + (free_sq) * (1 - _cb.checkers);


    if (legal_EnpassantPawns(l_pawns, r_pawns, Square(_cb.csep & 127)))
        return true;
    
    if (legal_PromotionPawns(e_pawns, move_sq, capt_sq))
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

template <Color c_my>
static bool
LegalPieceMoves(const ChessBoard &_cb)
{
    const Bitboard pinned_pieces = LegalPinnedPieces<c_my>(_cb);
    const Bitboard filter =
        _cb.checkers * _cb.legalSquaresMaskInCheck + (1 - _cb.checkers) * AllSquares;

    if (pinned_pieces & 1)
        return true;

    Bitboard  my_pieces = _cb.piece<c_my, ALL>();
    Bitboard all_pieces = _cb.All();

    const auto canMove = [&] (const auto &__f, Bitboard piece)
    {
        piece &= ~pinned_pieces;
        Bitboard legal_squares = 0;

        while (piece != 0)
            legal_squares |= __f(NextSquare(piece), my_pieces, all_pieces) & filter;

        return (legal_squares != 0);
    };

    return LegalPawnMoves<c_my>(_cb, pinned_pieces, _cb.legalSquaresMaskInCheck)
        or canMove(KnightLegalSquares, _cb.piece<c_my, KNIGHT>())
        or canMove(BishopLegalSquares, _cb.piece<c_my, BISHOP>())
        or canMove(  RookLegalSquares, _cb.piece<c_my, ROOK  >())
        or canMove( QueenLegalSquares, _cb.piece<c_my, QUEEN >());
}

template <Color c_my>
static bool
LegalKingMoves(const ChessBoard& _cb, Bitboard attacked_squares)
{
    Square kpos = SquareNo(_cb.piece<c_my, KING>());
    Bitboard K_sq = plt::KingMasks[kpos];

    Bitboard legal_squares = K_sq & (~(_cb.piece<c_my, ALL>() | attacked_squares));

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
IsPassedPawn(Square idx, const ChessBoard& _cb)
{
    if (_cb.color == Color::WHITE)
    {
        Bitboard res = plt::UpMasks[idx];
        if ((idx & 7) >= 1) res |= plt::UpMasks[idx - 1];
        if ((idx & 7) <= 6) res |= plt::UpMasks[idx + 1];
        if (res & _cb.piece<BLACK, PAWN>()) return false;
        return true;
    }

    Bitboard res = plt::DownMasks[idx];
    if ((idx & 7) >= 1) res |= plt::DownMasks[idx - 1];
    if ((idx & 7) <= 6) res |= plt::DownMasks[idx + 1];
    if (res & _cb.piece<WHITE, PAWN>()) return false;
    return true;
}

bool
InterestingMove(Move move, const ChessBoard& _cb)
{
    Square ip = Square(move & 63);
    Square fp = Square((move >> 6) & 63);
    Square ep = Square(_cb.csep & 127);

    PieceType it = PieceType((move >> 12) & 7);

    Piece fpt = _cb.PieceOnSquare(fp);

    // If it captures a piece.
    if (fpt != Piece::NO_PIECE)
        return true;
    
    // An enpassant-move or a passed pawn
    if (it == PAWN) {
        if (fp == ep) return true;
        if (IsPassedPawn(ip, _cb)) return true;
    }

    // If its castling
    if ((it == KING) && std::abs(fp - ip) == 2) return true;

    // If the move gives a check.
    return ((move & (CHECK << 21)) != 0);
}

bool
FutilityPruneMove(Move move, ChessBoard& _cb)
{
    // Square ip = move & 63;
    Square fp = Square((move >> 6) & 63);

    PieceType it = PieceType((move >> 12) & 7);
    PieceType ft = PieceType((move >> 15) & 7);

    if (ft != NONE) return true;
    if (it == PAWN)
    {
        if (fp == (_cb.csep & 127)) return true;
        if (((1ULL << fp) & Rank18)) return true;
    }
    // _cb.MakeMove(move);
    // bool _InCheck = InCheck(_cb);
    // _cb.UnmakeMove();
    // if (_InCheck) return true;

    return false;
}


int
SearchExtension(const ChessBoard& pos, int numExtensions)
{
    if (numExtensions >= EXTENSION_LIMIT)
        return 0;

    if (pos.checkers > 1)
        return 1;

    return 0;
}

#endif

#ifndef GENERATOR


/**
 * @brief Calculates the bitboard of initial squares from which a moving piece
 * could potentially give a discovered check to the opponent's king.
**/
template <Color c_my>
static Bitboard
SquaresForDiscoveredCheck(const ChessBoard& pos)
{
    // Implementation to calculate and return the bitboard of squares.
    // These squares are the initial positions from which a piece could move
    // to potentially give a discovered check to the enemy king.

    constexpr Color c_emy = ~c_my;

    Square k_sq = SquareNo( pos.piece<c_emy, KING>() );

    Bitboard rq = pos.piece<c_my, QUEEN>() | pos.piece<c_my, ROOK  >();
    Bitboard bq = pos.piece<c_my, QUEEN>() | pos.piece<c_my, BISHOP>();

    Bitboard occupied = pos.All();
    Bitboard occupied_my = pos.piece<c_my, ALL>();
    Bitboard calculatedBitboard = 0;

    const auto checkSquare = [&] (const auto &__f, Bitboard* atk_table, Bitboard my_piece)
    {
        Bitboard mask = atk_table[k_sq] & occupied;
        Bitboard first_piece  = __f(mask);
        Bitboard second_piece = __f(mask ^ first_piece);

        if ((first_piece == 0) or (second_piece == 0))
            return;

        calculatedBitboard |=
            (((first_piece & occupied_my) != 0) and ((second_piece & my_piece) != 0)) ? first_piece : 0;
    };

    
    checkSquare(Lsb, plt::RightMasks, rq);
    checkSquare(Msb, plt::LeftMasks , rq);
    checkSquare(Lsb, plt::UpMasks   , rq);
    checkSquare(Msb, plt::DownMasks , rq);

    checkSquare(Lsb, plt::UpRightMasks  , bq);
    checkSquare(Lsb, plt::UpLeftMasks   , bq);
    checkSquare(Msb, plt::DownRightMasks, bq);
    checkSquare(Msb, plt::DownLeftMasks , bq);

    return calculatedBitboard;
}

template <Color c_my>
static void
GenerateSquaresThatCheckEnemyKing(const ChessBoard& pos, MoveList& myMoves)
{
    constexpr Color c_emy = ~c_my;

    Square k_sq = SquareNo( pos.piece<c_emy, KING>() );
    Bitboard occupied = pos.All();

    myMoves.squaresThatCheckEnemyKing[0] = plt::PawnCaptureMasks[c_emy][k_sq];
    myMoves.squaresThatCheckEnemyKing[1] = BishopAttackSquares(k_sq, occupied);
    myMoves.squaresThatCheckEnemyKing[2] = KnightAttackSquares(k_sq, occupied);
    myMoves.squaresThatCheckEnemyKing[3] =   RookAttackSquares(k_sq, occupied);
    myMoves.squaresThatCheckEnemyKing[4] =
        myMoves.squaresThatCheckEnemyKing[1] | myMoves.squaresThatCheckEnemyKing[3];
}


bool
LegalMovesPresent(ChessBoard& _cb)
{
    _cb.RemoveMovegenMetadata();

    if (_cb.color == WHITE)
    {
        KingAttackers<WHITE>(_cb);
        if ((_cb.checkers < 2) and LegalPieceMoves<WHITE>(_cb))
            return true;

        _cb.enemyAttackedSquares = GenerateAttackedSquares<WHITE>(_cb);
        return LegalKingMoves<WHITE>(_cb, _cb.enemyAttackedSquares);
    }

    KingAttackers<BLACK>(_cb);
    if ((_cb.checkers < 2) and LegalPieceMoves<BLACK>(_cb))
        return true;

    _cb.enemyAttackedSquares = GenerateAttackedSquares<BLACK>(_cb);
    return LegalKingMoves<BLACK>(_cb, _cb.enemyAttackedSquares);
}


template <Color c_my>
static inline MoveList
MoveGenerator(ChessBoard& pos, bool qsSearch, bool checks)
{
    MoveList myMoves(c_my, qsSearch);

    if (!pos.EnemyAttackedSquaresGenerated())
        pos.enemyAttackedSquares = GenerateAttackedSquares<c_my>(pos);

    if (!pos.AttackersFound())
        KingAttackers<c_my>(pos);

    if (pos.checkers < 2)
        PieceMovement<c_my>(pos, myMoves, pos.checkers);

    KingMoves<c_my>(pos, myMoves, pos.enemyAttackedSquares);

    if (checks)
    {
        myMoves.discoverCheckSquares = SquaresForDiscoveredCheck<c_my>(pos);
        GenerateSquaresThatCheckEnemyKing<c_my>(pos, myMoves);

        for (Move& move : myMoves)
            if (MoveGivesCheck<c_my>(move, pos, myMoves)) move |= CHECK << 21;
    }

    pos.RemoveMovegenMetadata();
    return myMoves;
}


MoveList
GenerateMoves(ChessBoard& pos, bool qsSearch, bool findChecks)
{
    return pos.color == WHITE ?
        MoveGenerator<WHITE>(pos, qsSearch, findChecks)
      : MoveGenerator<BLACK>(pos, qsSearch, findChecks);
}


#endif
