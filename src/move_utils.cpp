
#include "move_utils.h"


void
decode_move(Move move)
{
    Square ip  = move & 63;
    Square fp  = (move >> 6) & 63;
    PieceType pt  = PieceType((move >> 12) & 7);
    PieceType cpt = PieceType((move >> 15) & 7);
    Color cl  = Color((move >> 20) & 1);
    int ppt = (move >> 18) & 3;
    int pr  = (move >> 21) & 31;

    cout << "Start_Square : " << ip << '\n';
    cout << "End_Square : " << fp << '\n';
    cout << "Piece_type : " << pt << '\n';
    cout << "Cap_Piece_type : " << cpt << '\n';
    cout << "Piece_color : " << (cl == 1 ? "White\n" : "Black\n");
    cout << "Move_Strength : " << pr << '\n';

    if (pt == PAWN && ((1ULL << fp) & Rank18))
    {
        cout << "Promoted_to : ";
        if (ppt == 0) cout << "Bishop\n";
        else if (ppt == 1) cout << "Knight\n";
        else if (ppt == 2) cout << "Rook\n";
        else if (ppt == 3) cout << "Queen\n";
    }

    cout << endl;
}



inline Move
gen_base_move(const ChessBoard& _cb, Square ip)
{
    PieceType pt = type_of(_cb.piece_on_square(ip));
    int color_bit = _cb.color << 20;

    const Move base_move = color_bit + (pt << 12) + ip;
    return base_move;
}

inline void
add_cap_moves(Square ip, Bitboard endSquares,
    Move base_move, const ChessBoard& _cb, MoveList& myMoves)
{
    const auto cap_priority = [] (PieceType pt, PieceType cpt)
    { return (cpt - pt + 16); };

    const auto encode_full_move = [] (Move base,
        Square fp, PieceType cpt, int move_pr)
    {
        const Move e_move = base + (move_pr << 21) + (cpt << 15) + (fp << 6);
        return e_move;
    };

    PieceType pt = type_of(_cb.piece_on_square(ip));

    while (endSquares != 0)
    {
        Square fp = next_idx(endSquares);
        PieceType fpt = type_of(_cb.piece_on_square(fp));
        int move_priority = cap_priority(pt, fpt);

        const Move enc_move = encode_full_move(base_move, fp, fpt, move_priority);
        myMoves.Add(enc_move);
    }
}

inline void
add_quiet_moves(Bitboard endSquares, Move base_move, MoveList& myMoves)
{
    const auto encode_full_move = [] (Move base, Square fp, int pr)
    {
        const Move e_move = base + (pr << 21) + (fp << 6);
        return e_move;
    };

    while (endSquares != 0)
    {
        int fp = next_idx(endSquares);
        int move_priority = 0;
        const Move enc_move = encode_full_move(base_move, fp, move_priority);
        myMoves.Add(enc_move);
    }
}

void
add_move_to_list(Square ip, Bitboard endSquares, const ChessBoard& _cb, MoveList& myMoves)
{
    Bitboard emy_pieces = _cb.piece(~_cb.color, PieceType::ALL);
    if (myMoves.cpt_only)
        endSquares = endSquares & emy_pieces;

    Bitboard cap_Squares = endSquares & emy_pieces;
    Bitboard quiet_Squares = endSquares ^ cap_Squares;
    const Move base_move = gen_base_move(_cb, ip);

    add_cap_moves(ip, cap_Squares, base_move, _cb, myMoves);
    add_quiet_moves(quiet_Squares, base_move, myMoves);
}



void
add_quiet_pawn_moves(Bitboard endSquares,
    int shift, const ChessBoard& _cb, MoveList& myMoves)
{
    if (myMoves.cpt_only)
        return;

    PieceType pt = PAWN;
    int color_bit = _cb.color << 20;

    const Move base_move = color_bit + (pt << 12);

    while (endSquares != 0)
    {
        Square fp = next_idx(endSquares);
        Square ip = fp + shift;
        const Move enc_move = base_move + (fp << 6) + ip;
        myMoves.Add(enc_move);
    }
}


Bitboard
pawn_atk_sq(const ChessBoard& _cb, Color color)
{
    int inc  = 2 * int(color) - 1;
    Bitboard pawns = _cb.piece(color, PAWN);
    const auto shifter = (color == Color::WHITE) ? l_shift : r_shift;

    return shifter(pawns & RightAttkingPawns, 8 + inc) |
           shifter(pawns & LeftAttkingPawns , 8 - inc);
}

Bitboard
bishop_atk_sq(Square __pos, Bitboard _Ap)
{    
    uint64_t magic = plt::BishopMagics[__pos];
    int      bits  = plt::BishopShifts[__pos];
    Bitboard mask  = plt::BishopMasks[__pos];
    uint64_t start = plt::BishopStartIndex[__pos];

    uint64_t occupancy = (magic * (_Ap & mask)) >> bits;
    return plt::BishopMovesLookUp[start + occupancy];
}

Bitboard
knight_atk_sq(Square __pos, Bitboard _Ap)
{ return plt::KnightMasks[__pos] + (_Ap - _Ap); }

Bitboard
rook_atk_sq(Square __pos, Bitboard _Ap)
{
    uint64_t magic = plt::RookMagics[__pos];
    int      bits  = plt::RookShifts[__pos];
    Bitboard mask  = plt::RookMasks[__pos];
    uint64_t start = plt::RookStartIndex[__pos];

    uint64_t occupancy = (magic * (_Ap & mask)) >> bits;
    return plt::RookMovesLookUp[start + occupancy];
}

Bitboard
queen_atk_sq(Square __pos, Bitboard _Ap)
{ return bishop_atk_sq(__pos, _Ap) ^ rook_atk_sq(__pos, _Ap); }





CheckData
find_check_squares(const ChessBoard& _cb)
{
    Color c_my = _cb.color;
    Square kpos = idx_no(_cb.piece(c_my, KING));
    Bitboard Apieces = _cb.All();

    Bitboard res1 = rook_atk_sq(kpos, Apieces);
    Bitboard res2 = bishop_atk_sq(kpos, Apieces);
    Bitboard res3 = knight_atk_sq(kpos, Apieces);
    Bitboard res4 = plt::PawnCaptureMasks[c_my][kpos];

    return CheckData(res1, res2, res3, res4);
}


//  Checks if the king of the active side is in check.
bool
in_check(const ChessBoard& _cb)
{
    Color c_emy = ~_cb.color;
    CheckData check = find_check_squares(_cb);

    return (check.LineSquares     & (_cb.piece(c_emy, ROOK  ) | _cb.piece(c_emy, QUEEN)))
        or (check.DiagonalSquares & (_cb.piece(c_emy, BISHOP) | _cb.piece(c_emy, QUEEN)))
        or (check.KnightSquares   &  _cb.piece(c_emy, KNIGHT))
        or (check.PawnSquares     &  _cb.piece(c_emy, PAWN  ));
}


string
print(Move move, ChessBoard _cb)
{
    if (move == NULL_MOVE)
        return string("null");

    const auto IndexToRow = [] (int row)
    { return string(1, char(row + 49)); };

    const auto IndexToCol = [] (int col)
    { return string(1, char(col + 97)); };

    const auto IndexToSquare = [&] (int row, int col)
    { return IndexToCol(col) + IndexToRow(row); };

    const char piece_names[4] = {'B', 'N', 'R', 'Q'};

    Square ip =  move       & 63;
    Square fp = (move >> 6) & 63;

    int ip_col = ip & 7;
    int fp_col = fp & 7;

    int ip_row = (ip - ip_col) >> 3;
    int fp_row = (fp - fp_col) >> 3;

    PieceType  pt = PieceType((move >> 12) & 7);
    PieceType cpt = PieceType((move >> 15) & 7);

    Bitboard Apieces = _cb.All();

    _cb.MakeMove(move);
    string gives_check = in_check(_cb) ? "+" : "";
    _cb.UnmakeMove();

    string captures = (cpt != 0) ? "x" : "";

    if (pt == PAWN)
    {
        string pawn_captures
            = (std::abs(ip_col - fp_col) == 1)
            ? (IndexToCol(ip_col) + "x") : "";

        string dest_square = IndexToSquare(fp_row, fp_col);

        // No promotion
        if (((1ULL << fp) & Rank18) == 0)
            return pawn_captures + dest_square + gives_check;

        int ppt = (move >> 18) & 3;
        string promoted_piece = string("=") + string(1, piece_names[ppt]);

        return pawn_captures + dest_square + promoted_piece + gives_check;
    }

    if (pt == KING)
    {
        Bitboard fpos = 1ULL << fp;
        // Castling
        if (std::abs(ip_col - fp_col) == 2)
            return string((fpos & FileG) ? "O-O" : "O-O-O") + gives_check;

        return string("K") + captures + IndexToSquare(fp_row, fp_col) + gives_check;
    }

    // If piece is [BISHOP, KNIGHT, ROOK, QUEEN]
    Bitboard pieces =
        (pt == BISHOP ? bishop_atk_sq(fp, Apieces) :
        (pt == KNIGHT ? knight_atk_sq(fp, Apieces) :
        (pt == ROOK   ?   rook_atk_sq(fp, Apieces) :
        queen_atk_sq(fp, Apieces))));

    pieces = (pieces & _cb.piece(_cb.color, pt)) ^ (1ULL << ip);

    string piece_name = string(1, piece_names[pt - 2]);
    string end_part = captures + IndexToSquare(fp_row, fp_col) + gives_check;

    if (pieces == 0)
        return piece_name + end_part;
    
    bool row = true, col = true;

    while (pieces > 0)
    {
        Square __pos = next_idx(pieces);

        int __pos_col = __pos & 7;
        int __pos_row = (__pos - __pos_col) >> 3;

        if (__pos_col == ip_col) col = false;
        if (__pos_row == ip_row) row = false;
    }

    if (col) return piece_name + IndexToCol(ip_col) + end_part;
    if (row) return piece_name + IndexToRow(ip_row) + end_part;

    return piece_name + IndexToSquare(ip_row, ip_col) + end_part;
}


void
print(const MoveList& myMoves, const ChessBoard& _cb)
{
    cout << "MoveCount : " << myMoves.size() << '\n';
    
    int move_no = 0;
    for (const Move move : myMoves)
    {
        string result = print(move, _cb);
        cout << (++move_no) << " \t| "  << result
             << string(8 - result.size(), ' ') << "| " << move << '\n';
    }

    cout << endl;
}

