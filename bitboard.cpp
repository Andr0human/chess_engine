

#include "bitboard.h"

uint64_t tmp_total_counter = 0;
uint64_t tmp_this_counter = 0;

chessBoard::chessBoard() {
    color = 1;
    csep = moveNum = 0;
    Hash_Value = 0;
    halfmove = fullmove = 0;
    KA = -1;
    for (int i = 0; i < 64; i++) board[i] = 0;
    for (int i = 0; i < 16; i++) Pieces[i] = 0; 
}

chessBoard::chessBoard(const std::string &fen) {
    
    // Split the elements from fen.
    const auto text_split = [](const std::string &__s, char sep) {
        std::vector<std::string> res;
        size_t prev = 0, __n = __s.length();
        
        for (size_t i = 0; i < __n; i++) {
            if (__s[i] != sep) continue;
            
            if (i > prev) res.push_back(__s.substr(prev, i - prev));
            prev = i + 1;
        }

        if (__n > prev) res.push_back(__s.substr(prev, __n - prev));
        return res;
    };

    const auto char_to_piece_no = [] (const char ch) {
        int res = 1;
        const char piece = ch < 'a' ? ch : ch - 32; 

        if (piece == 'K') res = 6;
        else if (piece == 'Q') res = 5;
        else if (piece == 'R') res = 4;
        else if (piece == 'N') res = 3;
        else if (piece == 'B') res = 2;

        res += ch < 'a' ? 8 : 0;
        return res;
    };

    const auto elements = text_split(fen, ' ');
    
    {
        // Generating board and Pieces array
        int rank = 7, col = 0;
        for (const char ch : elements[0]) {
            if (ch >= '1' && ch <= '9') {
                col += ch - '0';
            } else if (ch == '/') {
                rank--;
                col = 0;
            } else {
                board[8 * rank + col] = char_to_piece_no(ch);
                col++;
            }
        }

        for (int i = 0; i < 64; i++)
            if (board[i]) Pieces[board[i]] |= 1ULL << i;
        
        for (int i = 1; i <= 6; i++) {
            Pieces[ 7] |= Pieces[i];
            Pieces[15] |= Pieces[8 + i];
        }
    
    }


    // Extracting which color to play
    color = (elements[1] == "w" ? 1 : 0);
    
    // Extracting castle-info
    csep = 0;
    for (char ch : elements[2]) {
        if (ch == 'K') csep |= 1024;
        else if (ch == 'Q') csep |= 512;
        else if (ch == 'k') csep |= 256;
        else if (ch == 'q') csep |= 128;
    }

    // Extracting en-passant square
    if (elements[3] == "-") csep |= 64;
    else csep |= 28 + ((2 * color - 1) * 12) + (elements[3][0] - 'a');

    // Extracting half-move and full-move
    if (elements.size() == 6) {
        halfmove = std::stoi(elements[4]);
        fullmove = std::stoi(elements[5]);
    } else {
        halfmove = 0;
        fullmove = 1;
    }

    KA = -1;
    // Generate hash-value for current position
    Hash_Value = generate_hashKey();
}

void chessBoard::MakeMove(int move) {

    const int ip = move & 63, fp = (move >> 6) & 63;
    const int ep = csep & 127;
    int pt = board[ip], cpt = board[fp];
    const uint64_t iPos = 1ULL << ip, fPos = 1ULL << fp;
    
    const int own = color << 3, emy = (color ^ 1) << 3;

    const int piece_cl = (color << 1) - 1;
    const int pt_m = (pt & 7) * piece_cl;
    const int cpt_m = (cpt & 7) * piece_cl;


    aux_table_move[moveNum] = move;
    aux_table_csep[moveNum] = csep;
    aux_table_hash[moveNum] = Hash_Value;
    moveNum++;

    board[ip] = 0;
    board[fp] = pt;
    csep = (csep & 1920) ^ 64;
 
    if ((pt & 7) == 1) {
        if (fp == ep) {
            // Check if captured square is an en-passant square
            
            const int pawn_fp = ep - 8 * piece_cl;
            Pieces[emy + 1] ^= 1ULL << (pawn_fp);
            Pieces[emy + 7] ^= 1ULL << (pawn_fp);
            board[pawn_fp] = 0;

            #if defined(TRANSPOSITION_TABLE_H)
                Hash_Value ^= TT.hash_key(pieceOfs - pt_m * (pawn_fp + 1));    // Fix needed
            #endif
        }
        else if (std::abs(fp - ip) == 16) {
            // Check if pawns move 2 two steps up or down
            // Generates en-passant square
            
            csep = (csep & 1920) | ((fp + ip) >> 1);           // Fix needed
        }
        else if (fPos & 0xFF000000000000FF) {
            // Check for Pawn Promotion

            Pieces[pt] ^= iPos;
            #if defined(TRANSPOSITION_TABLE_H)
                Hash_Value ^= TT.hash_key(pieceOfs + pt_m * (ip + 1));
            #endif

            pt = (((move >> 18) & 3) + 2) + own;
            board[fp] = pt;
            Pieces[pt] ^= iPos;
            
            const int new_pt_m = (pt & 7) * piece_cl;

            #if defined(TRANSPOSITION_TABLE_H)
                Hash_Value ^= TT.hash_key(pieceOfs + new_pt_m * (ip + 1));
            #endif
        }
    }
    else if ((pt & 7 )== 6) {  /// Check for a king Move

        const int filter_value = 2047 ^ (384 << (color * 2));
        csep &= filter_value;

        if (std::abs(fp - ip) == 2) {

            const uint64_t swap_value = ((fp > ip ? 160ULL : 9ULL) << (56 * (color ^ 1)));

            Pieces[own + 4] ^= swap_value;
            Pieces[own + 7] ^= swap_value;

            if (fp > ip) {
                board[ip + 3] = 0;
                board[ip + 1] = own + 4;
            } else {
                board[ip - 4] = 0;
                board[ip - 1] = own + 4;
            }
        }

    }

    ////// Check if a rook got captured
    if ((fPos & 0x8100000000000081) && (move & 229376) == 131072) {
        if (fp == 0) csep &= 1535;
        else if (fp == 7) csep &= 1023;
        else if (fp == 56) csep &= 1919;
        else if (fp == 63) csep &= 1791;
    }

    ///// CHECK if a Rook Moved
    if ((iPos & 0x8100000000000081) && (move & 28672) == 16384) {
        if (ip == 0) csep &= 1535;
        else if (ip == 7) csep &= 1023;
        else if (ip == 56) csep &= 1919;
        else if (ip == 63) csep &= 1791;
    }


    if (cpt != 0) {
        Pieces[cpt] ^= fPos;
        Pieces[emy + 7] ^= fPos;

        #if defined(TRANSPOSITION_TABLE_H)
            Hash_Value ^= TT.hash_key(pieceOfs + cpt_m * (fp + 1));
        #endif
    }

    Pieces[pt] ^= iPos ^ fPos;
    Pieces[own + 7] ^= iPos ^ fPos;

    #if defined(TRANSPOSITION_TABLE_H)
        Hash_Value ^= TT.hash_key(pieceOfs + pt_m * (ip + 1)) ^ TT.hash_key(0)
                    ^ TT.hash_key(pieceOfs + pt_m * (fp + 1));
    #endif
    color ^= 1;
}

void chessBoard::UnmakeMove() {
    
    if (moveNum <= 0) return;
    moveNum--;
    int move   = aux_table_move[moveNum];
    int t_csep = aux_table_csep[moveNum];

    color ^= 1;
    const int ip = move & 63, fp = (move >> 6) & 63;
    const int ep = t_csep & 127;
    
    const uint64_t iPos = 1ULL << ip, fPos = 1ULL << fp;
    const int own = color << 3, emy = (color ^ 1) << 3;
    int pt = ((move >> 12) & 7) ^ own, cpt = ((move >> 15) & 7) ^ emy;

    const int piece_cl = (color << 1) - 1;

    if (cpt == 8) cpt = 0;

    board[ip] = pt;
    board[fp] = cpt;

    if (cpt) {
        Pieces[cpt] ^= fPos;
        Pieces[emy + 7] ^= fPos;  
    }
    
    Pieces[pt] ^= iPos ^ fPos;
    Pieces[own + 7] ^= iPos ^ fPos;

    if ((pt & 7) == 1) {
        if (fp == ep) {
            const int pawn_fp = ep - 8 * piece_cl;
            Pieces[emy + 1] ^= 1ULL << (pawn_fp);
            Pieces[emy + 7] ^= 1ULL << (pawn_fp);
            board[pawn_fp] = emy + 1;
        }
        else if ((fPos & 0xFF000000000000FF)) {
            pt = (((move >> 18) & 3) + 2) + own;
            Pieces[own + 1] ^= fPos;
            Pieces[pt] ^= fPos;
        }
    }
    else if ((pt & 7) == 6 && std::abs(fp - ip) == 2) {  /// Check for a king Move

        const uint64_t swap_value = (fp > ip ? 160ULL : 9ULL) << (56 * (color ^ 1));
        Pieces[own + 4] ^= swap_value;
        Pieces[own + 7] ^= swap_value;

        if (fp > ip) {
            board[ip + 3] = own + 4;
            board[ip + 1] = 0 ;
        } else {
            board[ip - 4] = own + 4;
            board[ip - 1] = 0;
        }

    }
    
    csep = t_csep;
    Hash_Value = aux_table_hash[moveNum];
}

const std::string chessBoard::fen() const {
    
    std::string generated_fen;
    int zero = 0;

    const auto piece_no_to_char = [] (const int piece_no) {
        char res = 'P';
        int piece = piece_no & 7;

        if (piece == 6) res = 'K';
        else if (piece == 5) res = 'Q';
        else if (piece == 4) res = 'R';
        else if (piece == 3) res = 'N';
        else if (piece == 2) res = 'B';

        if ((piece_no & 8) == 0) res += 32;
        return res;
    };
    
    for(int j = 7; j >= 0; j--) {
        for (int i = 0; i < 8; i++) {
            if (board[8 * j + i] == 0) zero++;
            else {
                if (zero) {
                    generated_fen.push_back(static_cast<char>(zero + 48));      // '0' + zero
                    zero = 0;
                }
                generated_fen.push_back(piece_no_to_char(board[8 * j + i]));
            }
        }
        if (zero != 0) {
            generated_fen.push_back(static_cast<char>(zero + 48));              // '0' + zero
            zero = 0;
        }
        if (j != 0) generated_fen.push_back('/');
    }

    generated_fen += " ";
    generated_fen += (color == 1) ? "w " : "b ";

    if (csep & 1024) generated_fen.push_back('K');
    if (csep &  512) generated_fen.push_back('Q');
    if (csep &  256) generated_fen.push_back('k');
    if (csep &  128) generated_fen.push_back('q');
    if ((csep & 1920) == 0) generated_fen.push_back('-');
    generated_fen.push_back(' ');

    if ((csep & 64) != 0) generated_fen += "- ";
    else {
        generated_fen.push_back((csep & 7) + 'a');
        generated_fen += (color == 1 ? "6 " : "3 ");
    }

    generated_fen += std::to_string(halfmove) + " " +
                     std::to_string(fullmove);

    return generated_fen;
}

uint64_t chessBoard::generate_hashKey() const {
    
    uint64_t key = 0;

    #if defined(TRANSPOSITION_TABLE_H)

        const int castle_offset = 66;
        if (color == 0) key ^= TT.hash_key(0);

        key ^= TT.hash_key((csep & 127) + 1);
        key ^= TT.hash_key((csep >> 7) + castle_offset);

        for (int i = 1; i < 16; i++) {
            
            const int mul = i > 8 ? 1 : -1;
            uint64_t tmp = Pieces[i];
            
            while (tmp) {
                const int idx = lSb_idx(tmp);
                tmp &= tmp - 1;
                key ^= TT.hash_key(pieceOfs + i * (idx + 1) * mul);
            }
        }

    #endif

    return key;
}

void chessBoard::MakeNullMove() {
    csep = (csep & 1920) ^ 64;
    // Hash_Value ^= TT.HashIndex[0];
    color ^= 1;
}

void chessBoard::UnmakeNullMove() {
    // Update to not use t_csep form external source
    // csep = t_csep;
    // Hash_Value ^= TT.HashIndex[0];
    color ^= 1;
    // moveInvert(0, t_csep, t_HashVal);
}

void chessBoard::Reset() { 
    for (int i = 0; i < 64; i++) board[i] = 0;
    for (int i = 0; i < 16; i++) Pieces[i] = 0;
    csep = 0;
    Hash_Value = 0;
    moveNum = 0;
}

void chessBoard::fill_with_piece(std::string arr[], uint64_t value, char ch) const {
    
    while (value != 0) {
        const int idx = lSb_idx(value);
        value &= value - 1;

        const int x = idx & 7, y = (idx - x) >> 3;
        arr[x][y] = ch;
    }
}

void chessBoard::show() const {
    
    string arr[8];
    for (int i = 0; i < 8; i++) arr[i] = "........";

    const char _piece[16] = {
        '.', 'p', 'b', 'n', 'r', 'q', 'k', '.',
        '.', 'P', 'B', 'N', 'R', 'Q', 'K', '.'
    };

    for (int i = 1; i < 15; i++) {
        if (i == 7) continue;
        fill_with_piece(arr, Pieces[i], _piece[i]);
    }

    const string s = "+---+---+---+---+---+---+---+---+\n";
    cout << s;
    for (int j = 7; j >= 0; j--) {
        cout << "| ";
        for (int i = 0; i < 8; i++)
            cout << arr[i][j] << " | ";
        cout << '\n';
        cout << s;
    }
    cout << endl;
}

bool chessBoard::operator== (const chessBoard& other) {

    for (int i = 0; i < 64; i++)
        if (board[i] != other.board[i]) return false;

    for (int i = 0; i < 16; i++)
        if (Pieces[i] != other.Pieces[i]) return false;
    
    if (csep != other.csep) return false;
    if (color != other.color) return false;

    if (Hash_Value != other.Hash_Value) return false;

    return true;
}

bool chessBoard::operator!= (const chessBoard& other) {
    return !(*this == other);
}
