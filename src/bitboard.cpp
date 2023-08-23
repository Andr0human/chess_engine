

#include "bitboard.h"

uint64_t tmp_total_counter = 0;
uint64_t tmp_this_counter = 0;

ChessBoard::ChessBoard()
{
    color = Color::WHITE;
    csep = moveNum = 0;
    Hash_Value = 0;
    halfmove = fullmove = 0;
    KA = -1;
    for (Square i = 0; i < 64; i++) board[i] = NO_PIECE;
    for (int i = 0; i < 16; i++) piece_bb[i] = 0; 
}

ChessBoard::ChessBoard(const string& fen)
{ set_position_with_fen(fen); }

void
ChessBoard::set_position_with_fen(const string& fen) noexcept
{
    using std::stoi;

    const auto char_to_piece_type = [] (char ch)
    {
        const int piece_no[8] = {6, 2, 0, 3, 0, 1, 5, 4};

        const char piece = ch < 'a' ? ch : ch - 32;
        int v = static_cast<int>(piece);

        int index = ((v - 3) % 10) - 2;
        return
            piece_no[index] + (ch < 'a' ? 8 : 0);
    };

    const auto castling_rights = [&] (char ch)
    {
        if (ch == '-')
            return;

        int v = static_cast<int>(ch);
        csep |= 1ULL << (10 - (v % 5));
    };

    Reset();

    // Split the elements from FEN.
    const vector<string> elements = base_utils::split(fen, ' ');
    
    {
        // Generating board and Pieces array

        int square = 56;
        for (const char elem : elements[0])
        {
            if (isdigit(elem))
                square += static_cast<int>(elem - '0');
            else if (elem == '/')
                square -= (square & 7) ? ((square & 7) + 8) : (16);
            else
            {
                Piece __x = Piece(char_to_piece_type(elem));
                board[square] = __x;
                piece_bb[__x] |= 1ULL << square;
                piece_bb[(__x & 8) + 7] |= 1ULL << square;
                square++;
            }
        }
    }

    // Extracting which color to play
    color = Color(int(elements[1][0]) & 1);

    // Extracting castle-info
    for (char ch : elements[2])
        castling_rights(ch);

    // Extracting en-passant square
    if (elements[3] == "-") csep |= 64;
    else csep |= 28 + ((2 * color - 1) * 12) + (elements[3][0] - 'a');

    // Extracting half-move and full-move
    if (elements.size() == 6)
    {
        halfmove = stoi(elements[4]);
        fullmove = stoi(elements[5]) * 2 + (color ^ 1);
    }

    // Generate hash-value for current position
    Hash_Value = generate_hashKey();
}

const string
ChessBoard::fen() const
{
    const auto piece_no_to_char = [] (int piece_no)
    {
        int pvalues[7] = {0, 80, 66, 78, 82, 81, 75};

        int v = pvalues[piece_no & 7]
              + ((piece_no & 8) ^ 8) * 4;

        return static_cast<char>(v);
    };

    const auto add_zeros_to_fen = [] (string& __s, int& zeros)
    {
        if (zeros == 0)
            return;
        
        __s.push_back(static_cast<char>(zeros + 48));
        zeros = 0;
    };

    const auto add_castle_to_fen = [&] (string& __s)
    {
        if ((csep & 1920) == 0)
            return __s.push_back('-');

        for (int i = 0; i < 4; i++)
        {
            int state = ((i & 1) *  6)
                      + ((i & 2) * 16) + 75;
            
            if ((1ULL << (10 - i)) & csep)
                __s.push_back(static_cast<char>(state));
        }
    };

    string generated_fen;
    int zero = 0;
    
    for(int j = 7; j >= 0; j--)
    {
        for (int i = 0; i < 8; i++)
        {
            if (board[8 * j + i] == 0)
                zero++;
            else
            {
                add_zeros_to_fen(generated_fen, zero);
                generated_fen.push_back(piece_no_to_char(board[8 * j + i]));
            }
        }

        add_zeros_to_fen(generated_fen, zero);
        generated_fen.push_back('/');
    }

    generated_fen.pop_back();

    generated_fen += " ";
    generated_fen += (color == 1) ? "w " : "b ";

    add_castle_to_fen(generated_fen);
    generated_fen.push_back(' ');

    if ((csep & 64) != 0)
        generated_fen += "- ";
    else
    {
        generated_fen.push_back((csep & 7) + 'a');
        generated_fen += (color == 1 ? "6 " : "3 ");
    }

    generated_fen += std::to_string(halfmove) + " " +
                     std::to_string(fullmove >> 1);

    return generated_fen;
}


void
ChessBoard::MakeMove(Move move, bool in_search) noexcept
{
    // Init and Dest. sq
    Square ip = move & 63;
    Square fp = (move >> 6) & 63;

    Bitboard iPos = 1ULL << ip;
    Bitboard fPos = 1ULL << fp;

    int ep = csep & 127;
    int own = color << 3;
    int emy = (color ^ 1) << 3;

    PieceType it = PieceType((move >> 12) & 7);
    PieceType ft = PieceType((move >> 15) & 7);

    // Piece at init and dest. sq.
    Piece ipt = board[ip];
    Piece fpt = board[fp];

    UndoInfoPush(it, ft, move, in_search);

    halfmove = (ft != PieceType::NONE) or (it == PieceType::PAWN) ? 0 : halfmove + 1;
    fullmove++;

    board[ip] = NO_PIECE;
    board[fp] = ipt;

    #if defined(TRANSPOSITION_TABLE_H)

        if (ep != 64)
            Hash_Value ^= TT.hash_key(ep + 1);

    #endif

    // Reset the en-passant state
    csep = (csep & 1920) ^ 64;

    // Check if a rook is on dest. sq.
    make_move_castle_check(ft, fp);

    // Check if a rook is on init. sq.
    make_move_castle_check(it, ip);

    // Check for pawn special moves.
    if (it == PieceType::PAWN)
    {
        if (is_double_pawn_push(ip, fp))
            return make_move_double_pawn_push(ip, fp);

        if (is_en_passant(fp, ep))
            return make_move_enpassant(ip, fp);

        if (is_pawn_promotion(fp))
            return make_move_pawn_promotion(move);
    }

    // Check for king moves.
    if (it == PieceType::KING)
    {
        int old_csep = csep;
        int filter = 2047 ^ (384 << (color * 2));
        csep &= filter;

        update_csep(old_csep, csep);

        if (is_castling(ip, fp))
            return make_move_castling(ip, fp, 1);
    }

    if (fpt != 0)
    {
        piece_bb[fpt] ^= fPos;
        piece_bb[emy + 7] ^= fPos;

        #if defined(TRANSPOSITION_TABLE_H)
            Hash_Value ^= TT.hashkey_update(fpt, fp);
        #endif
    }

    piece_bb[ipt] ^= iPos ^ fPos;
    piece_bb[own + 7] ^= iPos ^ fPos;

    // Flip the sides.
    color = ~color;

    #if defined(TRANSPOSITION_TABLE_H)
        Hash_Value ^= TT.hashkey_update(ipt, ip)
                    ^ TT.hashkey_update(ipt, fp)
                    ^ TT.hash_key(0);
    #endif
}

void
ChessBoard::update_csep(int old_csep, int new_csep) noexcept
{
    #if defined(TRANSPOSITION_TABLE_H)
        Hash_Value ^= TT.hash_key((old_csep >> 7) + 66);
        Hash_Value ^= TT.hash_key((new_csep >> 7) + 66);
    #endif
}

void
ChessBoard::make_move_castle_check(PieceType piece, Square sq) noexcept
{
    // piece - at (init) or (dest) square.

    Bitboard CORNER_SQUARES = 0x8100000000000081;
    Bitboard bit_pos = 1ULL << sq;

    if ((bit_pos & CORNER_SQUARES) and (piece == PieceType::ROOK))
    {
        int old_csep = csep;
        int y = (sq + 1) >> 3;
        int z  = y + (y < 7 ? 9 : 0);
        csep &= 2047 ^ (1 << z);

        update_csep(old_csep, csep);
    }
}

void
ChessBoard::make_move_double_pawn_push(Square ip, Square fp) noexcept
{
    int own = color << 3;
    csep = (csep & 1920) | ((ip + fp) >> 1);

    piece_bb[own + 1] ^= (1ULL << ip) ^ (1ULL << fp);
    piece_bb[own + 7] ^= (1ULL << ip) ^ (1ULL << fp);

    #if defined(TRANSPOSITION_TABLE_H)
        // Add current enpassant-state to hash_value
        Hash_Value ^= TT.hash_key(1 + (csep & 127));
        Hash_Value ^= TT.hashkey_update(own + 1, ip)
                    ^ TT.hashkey_update(own + 1, fp)
                    ^ TT.hash_key(0);
    #endif

    color = ~color;
}

void
ChessBoard::make_move_enpassant(Square ip, Square ep) noexcept
{
    int own = color << 3;
    int emy = own ^ 8;
    Square cap_pawn_fp = ep - 8 * (2 * color - 1);

    // Remove opp. pawn from the Pieces-table
    piece_bb[emy + 1] ^= 1ULL << cap_pawn_fp;
    piece_bb[emy + 7] ^= 1ULL << cap_pawn_fp;
    board[cap_pawn_fp] = NO_PIECE;

    // Shift own pawn in Pieces-table
    piece_bb[own + 1] ^= (1ULL << ip) ^ (1ULL << ep);
    piece_bb[own + 7] ^= (1ULL << ip) ^ (1ULL << ep);

    color = ~color;

    #if defined(TRANSPOSITION_TABLE_H)
        Hash_Value ^= TT.hashkey_update(emy + 1, cap_pawn_fp);
        Hash_Value ^= TT.hashkey_update(own + 1, ip)
                    ^ TT.hashkey_update(own + 1, ep)
                    ^ TT.hash_key(0);
    #endif
}

void
ChessBoard::make_move_pawn_promotion(Move move) noexcept
{
    Square ip  = move & 63;
    Square fp  = (move >> 6) & 63;
    PieceType cpt = PieceType((move >> 15) & 7);

    int own = color << 3;
    int emy = own ^ 8;
    PieceType new_pt = PieceType(((move >> 18) & 3) + 2);

    piece_bb[own + 1] ^= 1ULL << ip;
    piece_bb[own + new_pt] ^= 1ULL << fp;
    piece_bb[own + 7] ^= (1ULL << ip) ^ (1ULL << fp);
    // board[fp] = own + new_pt;
    board[fp] = make_piece(color, new_pt);

    if (cpt > 0)
    {
        piece_bb[emy + cpt] ^= 1ULL << fp;
        piece_bb[emy +   7] ^= 1ULL << fp;

        #if defined(TRANSPOSITION_TABLE_H)
            Hash_Value ^= TT.hashkey_update(emy + cpt, fp);
        #endif
    }

    color = ~color;

    #if defined(TRANSPOSITION_TABLE_H)
        Hash_Value ^= TT.hashkey_update(own + 1, ip);
        Hash_Value ^= TT.hashkey_update(own + new_pt, fp);
        Hash_Value ^= TT.hash_key(0);
    #endif
}

void
ChessBoard::make_move_castling(Square ip, Square fp, int call_from_makemove) noexcept
{
    int own = int(color) << 3;
    
    Bitboard rooks_indexes =
            (fp > ip ? 160ULL : 9ULL) << (56 * (~color));

    piece_bb[own + 4] ^= rooks_indexes;
    piece_bb[own + 7] ^= rooks_indexes;

    int flask = (!call_from_makemove) * (own + 4);

    if (fp > ip)
    {
        board[ip + 3] = Piece(flask);
        board[ip + 1] = Piece((own + 4) ^ flask);
    }
    else
    {
        board[ip - 4] = Piece(flask);
        board[ip - 1] = Piece((own + 4) ^ flask);
    }

    if (call_from_makemove)
    {
        piece_bb[own + 6] ^= (1ULL << ip) ^ (1ULL << fp);
        piece_bb[own + 7] ^= (1ULL << ip) ^ (1ULL << fp);
        color = ~color;


        #if defined(TRANSPOSITION_TABLE_H)
            int __p1 = lSb_idx(rooks_indexes);
            int __p2 = mSb_idx(rooks_indexes);
            Hash_Value ^= TT.hashkey_update(own + 6, ip) ^ TT.hashkey_update(own + 6, fp);
            Hash_Value ^= TT.hashkey_update(own + 4, __p1) ^ TT.hashkey_update(own + 4, __p2);
            Hash_Value ^= TT.hash_key(0);
        #endif
    }
}

void
ChessBoard::UnmakeMove() noexcept
{
    if (moveNum <= 0) return;

    color = ~color;
    Move move = UndoInfoPop();
    fullmove--;

    Square ip = move & 63;
    Square fp = (move >> 6) & 63;
    Square ep = csep & 127;
    
    Bitboard iPos = 1ULL << ip;
    Bitboard fPos = 1ULL << fp;

    int own =  color << 3;
    int emy = (~color) << 3;

    PieceType it = PieceType((move >> 12) & 7);
    PieceType ft = PieceType((move >> 15) & 7);

    Piece ipt = make_piece( color, it);
    Piece fpt = make_piece(~color, ft);

    if (fpt == 8) fpt = NO_PIECE;

    board[ip] = ipt;
    board[fp] = fpt;

    if (fpt != NO_PIECE)
    {
        piece_bb[fpt] ^= fPos;
        piece_bb[emy + 7] ^= fPos;  
    }

    piece_bb[ipt] ^= iPos ^ fPos;
    piece_bb[own + 7] ^= iPos ^ fPos;

    if (it == PAWN)
    {
        if (fp == ep)
        {
            Square pawn_fp = ep - 8 * (2 * color - 1);
            piece_bb[emy + 1] ^= 1ULL << (pawn_fp);
            piece_bb[emy + 7] ^= 1ULL << (pawn_fp);
            board[pawn_fp] = Piece(emy + 1);
        }
        else if ((fPos & 0xFF000000000000FF) != 0)
        {
            ipt = Piece((((move >> 18) & 3) + 2) + own);
            piece_bb[own + 1] ^= fPos;
            piece_bb[ipt] ^= fPos;
        }
    }

    if ((it == KING) and is_castling(ip, fp))
        return make_move_castling(ip, fp, 0);
}


void
ChessBoard::UndoInfoPush(PieceType it, PieceType ft, Move move, bool in_search)
{
    if (!in_search and ((ft != NONE) or (it == PAWN)))
        moveNum = 0;

    undo_info[moveNum++] = UndoInfo(move, csep, Hash_Value, halfmove);
}


Move
ChessBoard::UndoInfoPop()
{
    moveNum--;
    csep = undo_info[moveNum].csep;
    Hash_Value = undo_info[moveNum].hash;
    halfmove = undo_info[moveNum].halfmove;
    return undo_info[moveNum].move;
}


void
ChessBoard::add_prev_board_positions(const vector<Key>& prev_keys) noexcept
{
    for (Key key : prev_keys)
        undo_info[moveNum++] = UndoInfo(0, 0, key, 0);
}

bool
ChessBoard::three_move_repetition() const noexcept
{
    int pos_count = 0;
    int last = std::max(0, moveNum - halfmove);

    for (int i = moveNum - 1; i >= last; i--)
        if (Hash_Value == undo_info[i].hash)
            ++pos_count;
    
    return pos_count >= 1;
}

bool
ChessBoard::fifty_move_rule_draw() const noexcept
{ return halfmove > 100; }

Key
ChessBoard::generate_hashKey() const
{    
    Key key = 0;

    #if defined(TRANSPOSITION_TABLE_H)

        int castle_offset = 66;
        if (color == 0)
            key ^= TT.hash_key(0);

        if ((csep & 127) != 64)
            key ^= TT.hash_key((csep & 127) + 1);

        key ^= TT.hash_key((csep >> 7) + castle_offset);

        for (int piece = 1; piece < 15; piece++)
        {
            if ((piece == 8) or (piece == 7))
                continue;

            Bitboard __tmp = piece_bb[piece];
            while (__tmp > 0)
            {
                Square __pos = lSb_idx(__tmp);
                __tmp &= __tmp - 1;
                key ^= TT.hashkey_update(piece, __pos);
            }
        }

    #endif

    return key;
}

void
ChessBoard::MakeNullMove()
{
    csep = (csep & 1920) ^ 64;
    // Hash_Value ^= TT.HashIndex[0];
    color = ~color;
}

void
ChessBoard::UnmakeNullMove()
{
    // Update to not use t_csep form external source
    // csep = t_csep;
    // Hash_Value ^= TT.HashIndex[0];
    color = ~color;
    // moveInvert(0, t_csep, t_HashVal);
}

void
ChessBoard::Reset()
{
    for (Square i = 0; i < 64; i++) board[i] = NO_PIECE;
    for (int i = 0; i < 16; i++) piece_bb[i] = 0;
    csep = 0;
    Hash_Value = 0;
    moveNum = 0;
    KA = -1;
    legal_squares_mask = 0;
    enemy_attacked_squares = 0;
    color = Color::WHITE;

    halfmove = 0;
    fullmove = 1;
}

string
ChessBoard::visual_board() const noexcept
{
    const char _piece[16] = {
        '.', 'p', 'b', 'n', 'r', 'q', 'k', '.',
        '.', 'P', 'B', 'N', 'R', 'Q', 'K', '.'
    };

    const string s = " +---+---+---+---+---+---+---+---+\n";

    string gap = " | ";
    string res = s;

    for (Square sq = 56; sq >= 0; sq++)
    {
        res += gap + _piece[board[sq]];
        if ((sq & 7) == 7)
        {
            sq -= 16;
            res += " |\n" + s;
        }
    }
    return res;
}

bool
ChessBoard::operator==(const ChessBoard& other)
{
    for (Square i = 0; i < 64; i++)
        if (board[i] != other.board[i]) return false;

    for (int i = 0; i < 16; i++)
        if (piece_bb[i] != other.piece_bb[i]) return false;
    
    if (csep != other.csep) return false;
    if (color != other.color) return false;

    if (Hash_Value != other.Hash_Value) return false;

    return true;
}

bool
ChessBoard::operator!= (const ChessBoard& other)
{ return !(*this == other); }

void
ChessBoard::dump(std::ostream& writer)
{
    writer << "POSITION_DUMP" << endl;

    writer << "board: ";
    for (int i = 0; i < 64; i++)
        writer << board[i] << ' ';
    writer << endl;

    writer << "piece_bb: ";
    for (int i = 0; i < 16; i++)
        writer << piece_bb[i] << ' ';
    writer << endl;

    writer << "Color: " << int(color) << endl;
    writer << "csep: " << csep << endl;
    writer << "halfmove: " << halfmove << endl;
    writer << "fullmove: " << fullmove << endl;
    writer << "key: " << Hash_Value << endl;
    writer << "KA: " << KA << endl;
    writer << "legaL_square_mask: " << legal_squares_mask << endl;
    writer << "enemy_attacked_squares: " << enemy_attacked_squares << endl;

    writer << "movenum: " << moveNum << endl;
    writer << "undo_info: \n";

    for (int i = 0; i < moveNum; i++)
        writer << undo_info[i].move << ", " << undo_info[i].hash
               << ", " << undo_info[i].csep << undo_info[i].halfmove << endl;

    writer << endl;
}

