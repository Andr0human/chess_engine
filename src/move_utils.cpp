
#include "move_utils.h"


void
decode_move(MoveType move)
{
    int ip  = move & 63;
    int fp  = (move >> 6) & 63;
    int pt  = (move >> 12) & 7;
    int cpt = (move >> 15) & 7;
    int ppt = (move >> 18) & 3;
    int cl  = (move >> 20) & 1;
    int pr  = (move >> 21) & 31;

    cout << "Start_Square : " << ip << '\n';
    cout << "End_Square : " << fp << '\n';
    cout << "Piece_type : " << pt << '\n';
    cout << "Cap_Piece_type : " << cpt << '\n';
    cout << "Piece_color : " << (cl == 1 ? "White\n" : "Black\n");
    cout << "Move_Strength : " << pr << '\n';

    if (pt == 1 && ((1ULL << fp) & Rank18))
    {
        cout << "Promoted_to : ";
        if (ppt == 0) cout << "Bishop\n";
        else if (ppt == 1) cout << "Knight\n";
        else if (ppt == 2) cout << "Rook\n";
        else if (ppt == 3) cout << "Queen\n";
    }

    cout << endl;
}



inline MoveType
gen_base_move(const ChessBoard& _cb, int ip)
{
    int pt =  _cb.board[ip] & 7;
    int side = _cb.color << 20;

    const MoveType base_move = side + (pt << 12) + ip;
    return base_move;
}

inline void
add_cap_moves(int ip, uint64_t endSquares,
    MoveType base_move, const ChessBoard& _cb, MoveList& myMoves)
{
    const auto cap_priority = [] (int pt, int cpt)
    { return (cpt - pt + 16); };

    const auto encode_full_move = [] (int base,
        int fp, int cpt, int move_pr)
    {
        const MoveType e_move = base + (move_pr << 21) + (cpt << 15) + (fp << 6);
        return e_move;
    };

    int pt = _cb.board[ip] & 7;
    // int base_str = 8;

    while (endSquares != 0)
    {
        int fp = next_idx(endSquares);
        int cpt = _cb.board[fp] & 7;
        int move_priority = cap_priority(pt, cpt);
        
        const MoveType enc_move = encode_full_move(base_move, fp, cpt, move_priority);
        myMoves.Add(enc_move);
    }
}

inline void
add_quiet_moves(uint64_t endSquares, MoveType base_move,
    const ChessBoard& _cb, MoveList& myMoves)
{
    const auto encode_full_move = [] (int base, int fp, int pr)
    {
        const MoveType e_move = base + (pr << 21) + (fp << 6);
        return e_move;
    };

    while (endSquares != 0)
    {
        int fp = next_idx(endSquares);
        int move_priority = 0;
        const MoveType enc_move = encode_full_move(base_move, fp, move_priority);
        myMoves.Add(enc_move);
    }
}

void
add_move_to_list(int ip, uint64_t endSquares,
    const ChessBoard& _cb, MoveList& myMoves)
{
    if (myMoves.cpt_only)
        endSquares = endSquares & ALL(EMY);

    uint64_t cap_Squares = endSquares & ALL(EMY);
    uint64_t quiet_Squares = endSquares ^ cap_Squares;
    const MoveType base_move = gen_base_move(_cb, ip);

    add_cap_moves(ip, cap_Squares, base_move, _cb, myMoves);
    add_quiet_moves(quiet_Squares, base_move, _cb, myMoves);
}



void
add_quiet_pawn_moves(uint64_t endSquares,
    int shift, const ChessBoard& _cb, MoveList& myMoves)
{
    if (myMoves.cpt_only)
        return;

    int pt = 1;
    int side = _cb.color << 20;

    const MoveType base_move = side + (pt << 12);

    while (endSquares != 0)
    {
        int fp = next_idx(endSquares);
        int ip = fp + shift;
        const MoveType enc_move = base_move + (fp << 6) + ip;
        myMoves.Add(enc_move);
    }
}


uint64_t
pawn_atk_sq(const ChessBoard& _cb, int side)
{
    int inc  = 2 * side - 1;
    uint64_t pawns = _cb.Pieces[8 * side + 1];
    const auto shifter = (side == 1) ? l_shift : r_shift;

    return shifter(pawns & RightAttkingPawns, 8 + inc) |
           shifter(pawns & LeftAttkingPawns , 8 - inc);
}

uint64_t
bishop_atk_sq(int __pos, uint64_t _Ap)
{    
    uint64_t magic = plt::BishopMagics[__pos];
    uint64_t bits  = plt::BishopShifts[__pos];
    uint64_t mask  = plt::BishopMasks[__pos];
    uint64_t start = plt::BishopStartIndex[__pos];

    uint64_t occupancy = (magic * (_Ap & mask)) >> bits;
    return plt::BishopMovesLookUp[start + occupancy];
}

uint64_t
knight_atk_sq(int __pos, uint64_t _Ap)
{ return plt::KnightMasks[__pos] + (_Ap - _Ap); }

uint64_t
rook_atk_sq(int __pos, uint64_t _Ap)
{
    uint64_t magic = plt::RookMagics[__pos];
    uint64_t bits  = plt::RookShifts[__pos];
    uint64_t mask  = plt::RookMasks[__pos];
    uint64_t start = plt::RookStartIndex[__pos];

    uint64_t occupancy = (magic * (_Ap & mask)) >> bits;
    return plt::RookMovesLookUp[start + occupancy];
}

uint64_t
queen_atk_sq(int __pos, uint64_t _Ap)
{ return bishop_atk_sq(__pos, _Ap) ^ rook_atk_sq(__pos, _Ap); }




/* CheckData
find_check_squares(const ChessBoard& _cb, bool own_king)
{
    int own_side = _cb.color ^ (!own_king);
    int kpos = idx_no(KING(own_side << 3));

    uint64_t Apieces = ALL_BOTH;

    uint64_t res1 = rook_atk_sq(kpos, Apieces);
    uint64_t res2 = bishop_atk_sq(kpos, Apieces);
    uint64_t res3 = knight_atk_sq(kpos, Apieces);
    uint64_t res4 = plt::PawnCaptureMasks[own_side][kpos];

    return CheckData(res1, res2, res3, res4);
}
 */

CheckData
find_check_squares(int kpos, int side, uint64_t Apieces)
{
    uint64_t res1 = rook_atk_sq(kpos, Apieces);
    uint64_t res2 = bishop_atk_sq(kpos, Apieces);
    uint64_t res3 = knight_atk_sq(kpos, Apieces);
    uint64_t res4 = plt::PawnCaptureMasks[side][kpos];

    return CheckData(res1, res2, res3, res4);
}


uint64_t apiece_after_makemove(MoveType move, uint64_t _Ap)
{
    using std::abs;
    int ip  = (move & 63);
    int fp  = (move >> 6) & 63;

    int pt  = (move >> 12) & 7;
    int cpt = (move >> 15) & 7;

    int side = (move >> 20) & 1;

    uint64_t swap_indexes = (1ULL << ip) ^ (1ULL << fp);

    if ((abs(fp - ip) == 7 or abs(fp - ip) == 9) and (cpt == 0))
    {
        int cap_pawn_fp = fp - 8 * (2 * side - 1);
        return _Ap ^ swap_indexes ^ (1ULL << cap_pawn_fp);
    }

    if (pt == 6 and abs(fp - ip) == 2)
    {
        uint64_t rooks_indexes =
            (fp > ip ? 160ULL : 9ULL) << (56 * (side ^ 1));
        
        return _Ap ^ rooks_indexes ^ swap_indexes;
    }

    if (cpt != 0)
        _Ap ^= (1ULL << fp);

    return _Ap ^ swap_indexes;
}


bool
move_gives_check(MoveType move, const ChessBoard& _cb)
{
    int ip = move & 63;
    int fp = (move >> 6) & 63;

    int pt = (move >> 12) & 7;

    uint64_t Apieces = ALL(WHITE) | ALL(BLACK);

    // For direct checks
    CheckData checks = find_check_squares(_cb.king_pos_emy(), _cb.side_emy(), Apieces);

    if (checks.squares_for_piece(pt) & (1ULL << fp))
        return true;

    return false;
}



// Returns if the king for the side to move is in check.
bool
in_check(const ChessBoard& _cb, bool own_king)
{
    int emy = (_cb.color ^ own_king) << 3;
    CheckData check = find_check_squares(_cb.king_pos(), _cb.side(), ALL(WHITE) | ALL(BLACK));

    return (check.LineSquares     & (  ROOK(emy) | QUEEN(emy)))
        or (check.DiagonalSquares & (BISHOP(emy) | QUEEN(emy)))
        or (check.KnightSquares   &  KNIGHT(emy) )
        or (check.PawnSquares     &    PAWN(emy) );
}


string
print(MoveType move, const ChessBoard& _cb)
{
    // In case of no move, return null
    if (move == 0)
        return string("null");

    const auto index_to_row = [] (int row)
    { return string(1, static_cast<char>(row + 49)); };

    const auto index_to_col = [] (int col)
    { return string(1, static_cast<char>(col + 97)); };

    const auto index_to_square = [&] (int row, int col)
    { return index_to_col(col) + index_to_row(row); };


    int  ip =  move       & 63;
    int  fp = (move >> 6) & 63;

    int ip_col = ip & 7;
    int fp_col = fp & 7;

    int ip_row = (ip - ip_col) >> 3;
    int fp_row = (fp - fp_col) >> 3;

    int  pt = ((move >> 12) & 7);
    int cpt = ((move >> 15) & 7);

    uint64_t Apieces = ALL(WHITE) | ALL(BLACK);


    // CheckData checks = find_check_squares(_cb, false);

    CheckData checks = find_check_squares(_cb.king_pos_emy(), _cb.side_emy(), Apieces);

    bool move_gives_check = checks.squares_for_piece(pt) & (1ULL << fp);
    string gives_check = move_gives_check ? "+" : "";


    uint64_t (*check_for_piece[4])(int, uint64_t) =
        {bishop_atk_sq, knight_atk_sq, rook_atk_sq, queen_atk_sq};

    const char piece_names[4] = {'B', 'N', 'R', 'Q'};


    const auto print_move_pawn = [&] ()
    {
        string pawn_captures
            = (std::abs(ip_col - fp_col) == 1)
            ? (index_to_col(ip_col) + "x") : "";

        string dest_square = index_to_square(fp_row, fp_col);

        // No promotion
        if (((1ULL << fp) & Rank18) == 0)
            return pawn_captures + dest_square + gives_check;

        int ppt = (move >> 18) & 3;
        string promoted_piece = string("=") + string(1, piece_names[ppt]);

        bool ppt_gives_check = checks.squares_for_piece(ppt + 2) & (1ULL << fp);

        return pawn_captures + dest_square
             + promoted_piece
             + (ppt_gives_check ? "+" : "");
    };

    const auto print_move_king = [&] (const string piece_name)
    {
        // Castling
        if (std::abs(ip_col - fp_col) == 2)
            return ((1ULL << fp) & FileG) ? string("O-O") : string("O-O-O");

        string captures = (cpt != 0) ? "x" : "";

        return piece_name + captures + index_to_square(fp_row, fp_col) + gives_check;
    };

    const auto print_move_piece = [&] (const auto& __func, string piece_name)
    {
        string captures    = (cpt != 0)       ? "x" : "";

        string end_part
            = captures + index_to_square(fp_row, fp_col) + gives_check;

        uint64_t pieces = (__func(fp, ALL_BOTH) & _cb.Pieces[OWN + pt]) ^ (1ULL << ip);

        if (pieces == 0)
            return piece_name + end_part;

        bool row = true, col = true;

        while (pieces > 0)
        {
            int __pos = next_idx(pieces);

            int __pos_col = __pos & 7;
            int __pos_row = (__pos - __pos_col) >> 3;

            if (__pos_col == ip_col) col = false;
            if (__pos_row == ip_row) row = false;
        }

        if (col)
            return piece_name + index_to_col(ip_col) + end_part;

        if (row)
            return piece_name + index_to_row(ip_row) + end_part;

        return piece_name
             + index_to_square(ip_row, ip_col)
             + end_part;
    };


    if (pt == 1)
        return print_move_pawn();

    if (pt == 6)
        return print_move_king("K");

    return print_move_piece(check_for_piece[pt - 2], string(1, piece_names[pt - 2]));
}


void
print(const MoveList& myMoves, const ChessBoard& _cb)
{
    cout << "MoveCount : " << myMoves.size() << '\n';
    
    int move_no = 0;
    for (const MoveType move : myMoves)
    {
        string result = print(move, _cb);
        cout << (++move_no) << " \t| "  << result
             << string(8 - result.size(), ' ') << "| " << move << '\n';
    }

    cout << endl;
}

