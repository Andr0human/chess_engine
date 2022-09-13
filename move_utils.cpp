

#include "move_utils.h"


void
decode_move(const int move)
{
    const int ip  = move & 63;
    const int fp  = (move >> 6) & 63;
    const int pt  = (move >> 12) & 7;
    const int cpt = (move >> 15) & 7;
    const int ppt = (move >> 18) & 3;
    const int cl  = (move >> 20) & 1;
    const int pr  = (move >> 21) & 31;

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



inline int
gen_base_move(const chessBoard &_cb, int ip)
{
    const int pt =  _cb.board[ip] & 7;
    const int side = _cb.color << 20;

    const int base_move = side + (pt << 12) + ip;
    return base_move;
}

inline void
add_cap_moves(const int ip, uint64_t endSquares,
    const int base_move, const chessBoard &_cb, MoveList &myMoves)
{
    const auto cap_priority = [] (const int pt, const int cpt)
    { return (cpt - pt + 12); };

    const auto encode_full_move = [] (const int base,
            const int fp, const int cpt, const int move_pr)
    {
        const int e_move = base + (move_pr << 21) + (cpt << 15) + (fp << 6);
        return e_move;
    };

    const int pt = _cb.board[ip] & 7;
    // const int base_str = 8;

    while (endSquares != 0)
    {
        const int fp = next_idx(endSquares);
        const int cpt = _cb.board[fp] & 7;
        const int move_priority = cap_priority(pt, cpt);
        
        const int enc_move = encode_full_move(base_move, fp, cpt, move_priority);
        myMoves.Add(enc_move);
    }
}

inline void
add_quiet_moves(uint64_t endSquares,
    const int base_move, const chessBoard &_cb, MoveList &myMoves)
{
    const auto encode_full_move = [] (const int base, const int fp, const int pr)
    {
        const int e_move = base + (pr << 21) + (fp << 6);
        return e_move;
    };

    while (endSquares  != 0)
    {
        const int fp = next_idx(endSquares);
        const int move_priority = 0;
        const int enc_move = encode_full_move(base_move, fp, move_priority);
        myMoves.Add(enc_move);
    }
}

void
add_move_to_list(const int ip, uint64_t endSquares,
    const chessBoard &_cb, MoveList &myMoves)
{
    const uint64_t cap_Squares = endSquares & ALL(EMY);
    const uint64_t quiet_Squares = endSquares ^ cap_Squares;
    const int base_move = gen_base_move(_cb, ip);

    add_cap_moves(ip, cap_Squares, base_move, _cb, myMoves);
    add_quiet_moves(quiet_Squares, base_move, _cb, myMoves);
}



void
add_quiet_pawn_moves(uint64_t endSquares,
    const int shift, const chessBoard &_cb, MoveList & myMoves)
{
    const int pt = 1;
    const int side = _cb.color << 20;

    const int base_move = side + (pt << 12);

    while (endSquares != 0)
    {
        const int fp = next_idx(endSquares);
        const int ip = fp + shift;
        const int enc_move = base_move + (fp << 6) + ip;
        myMoves.Add(enc_move);
    }
}


uint64_t
pawn_atk_sq(const chessBoard& _cb, const int enemy)
{
    const auto side = _cb.color ^ enemy;
    const auto inc  = 2 * side - 1;
    const auto pawns = _cb.Pieces[8 * side + 1];
    const auto shifter = side == 1 ? l_shift : r_shift;

    return shifter(pawns & RightAttkingPawns, 8 + inc) |
           shifter(pawns & LeftAttkingPawns , 8 - inc);
}

uint64_t
bishop_atk_sq(const int __pos, const uint64_t _Ap)
{
    const auto area = [__pos, _Ap] (const uint64_t *table, const auto &__func)
    { return table[__func(table[__pos] & _Ap)]; };

    return plt::diag_Board[__pos] ^
          (area(plt::ulBoard, lSb_idx) ^ area(plt::urBoard, lSb_idx)
         ^ area(plt::dlBoard, mSb_idx) ^ area(plt::drBoard, mSb_idx));
}

uint64_t
knight_atk_sq(const int __pos, const uint64_t _Ap)
{ return plt::NtBoard[__pos] + (_Ap - _Ap); }

uint64_t
rook_atk_sq(const int __pos, const uint64_t _Ap)
{
    const auto area = [__pos, _Ap] (const uint64_t *table, const auto &__func)
    { return table[__func(table[__pos] & _Ap)]; };

    return plt::line_Board[__pos] ^
          (area(plt::uBoard, lSb_idx) ^ area(plt::dBoard, mSb_idx)
         ^ area(plt::rBoard, lSb_idx) ^ area(plt::lBoard, mSb_idx));
}

uint64_t
queen_atk_sq(const int __pos, const uint64_t _Ap)
{ return bishop_atk_sq(__pos, _Ap) ^ rook_atk_sq(__pos, _Ap); }



bool
Incheck(const chessBoard &_cb)
{
    const int kpos = idx_no(KING(OWN));
    uint64_t res, Apieces = ALL_BOTH;
    const int emy = EMY;
    
    // (Rook + Queen) Check
    res = rook_atk_sq(kpos, Apieces);
    if ((res & (ROOK(emy) ^ QUEEN(emy)))) return true;

    // (Bishop + Queen) Check
    res = bishop_atk_sq(kpos, Apieces);
    if ((res & (BISHOP(emy) ^ QUEEN(emy)))) return true;

    // Knight Check
    res = knight_atk_sq(kpos, Apieces);
    if ((res & KNIGHT(emy))) return true;

    // Pawn Check
    res = plt::pcBoard[_cb.color][kpos];
    if ((res & PAWN(emy))) return true;

    return false;
}

string
print(int move, chessBoard &_cb)
{    
    string res;
    if (!move) return "null";

    const int ip = move & 63, fp = (move >> 6) & 63;
    const int ip_x = ip & 7, fp_x = fp & 7;
    const int ip_y = (ip - ip_x) >> 3, fp_y = (fp - fp_x) >> 3;
    const int _pt = ((move >> 12) & 7), _cpt = ((move >> 15) & 7);
    const int color = (_cb.board[ip] > 8 ? 1 : 0);
    const uint64_t Apieces = ALL_BOTH;

    const auto idx_to_row = [](int value)
    { return (char)('a' + value); };
    
    const auto idx_to_col = [](int value)
    { return (char)('1' + value); };
    
    const auto add_to_string = [&res](char a, char b)
    {
        res.push_back(a);
        res.push_back(b);
    };
    
    const auto fill_move = [&](uint64_t area, char piece)
    {
        bool row = true, col = true, found = false;

        res = piece;
        area &= _cb.Pieces[OWN + _pt];
        area ^= 1ULL << ip;

        while (area)
        {
            found = true;
            const int idx = next_idx(area);
            const int x = idx & 7, y = (idx - x) >> 3;
            if (x == ip_x) col = false;
            if (y == ip_y) row = false;
        }
        if (found)
        {
            if (col) res += idx_to_row(ip_x);
            else if (row) res += idx_to_col(ip_y);
            else add_to_string(idx_to_row(ip_x), idx_to_col(ip_y));
        }
        if (_cpt) res += 'x';
        add_to_string(idx_to_row(fp_x), idx_to_col(fp_y));
    };

    // Can ruin the whole search if passed an invalid move.
    _cb.MakeMove(move);
    bool checks = Incheck(_cb);
    _cb.UnmakeMove();
    
    if (_pt == 1)
    {
        bool enp = false;
        if (std::abs(fp_x - ip_x) == 1 && !_cpt) enp = true;
        
        if (_cpt || enp)
            add_to_string(idx_to_row(ip_x), 'x');

        add_to_string(idx_to_row(fp_x), idx_to_col(fp_y));

        int ppt = -1;
        if ((color == 1 && fp_y == 7) || (color == -1 && fp_y == 0))
            ppt = (move >> 18) & 3;
        if (!ppt) res += "=B";
        else if (ppt == 1) res += "=N";
        else if (ppt == 2) res += "=R";
        else if (ppt == 3) res += "=Q";
        
    }
    else if (_pt == 2)
    {
        fill_move(bishop_atk_sq(fp, Apieces), 'B');
    }
    else if (_pt == 3)
    {
        fill_move(knight_atk_sq(fp, Apieces), 'N');
    }
    else if (_pt == 4)
    {
        fill_move(rook_atk_sq(fp, Apieces), 'R');
    }
    else if (_pt == 5)
    {
        fill_move(queen_atk_sq(fp, Apieces), 'Q');
    }
    else if (_pt == 6)
    {
        if (abs(fp_x - ip_x) == 2)
        {
            if (fp == 2 || fp == 58) res = "O-O-O";
            else if (fp == 6 || fp == 62) res = "O-O";
        }
        else
        {
            res += "K";
            if (_cpt) res += 'x';
            res += idx_to_row(fp_x);
            res += static_cast<char>(fp_y + 49);
        }
    }
    
    if (checks) res += "+";
    return res;
}

void
print(const MoveList &myMoves, chessBoard &_cb)
{
    cout << "MoveCount : " << myMoves.size() << '\n';
    
    int move_no = 0;
    for (const auto move : myMoves)
        cout << (++move_no) << "\t| " << print(move, _cb) << "\t| "
             << move << '\n';

    cout << endl;
}

