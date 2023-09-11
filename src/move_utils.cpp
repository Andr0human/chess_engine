
#include "move_utils.h"


void
DecodeMove(Move move)
{
    Square ip  = Square(move & 63);
    Square fp  = Square((move >> 6) & 63);

    PieceType pt  = PieceType((move >> 12) & 7);
    PieceType cpt = PieceType((move >> 15) & 7);
    Color cl  = Color((move >> 20) & 1);

    int ppt = (move >> 18) & 3;
    int pr  = move >> 21;

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



Bitboard
PawnAttackSquares(const ChessBoard& _cb, Color color)
{
    int inc  = 2 * int(color) - 1;
    Bitboard pawns = _cb.piece(color, PAWN);
    const auto shifter = (color == Color::WHITE) ? LeftShift : RightShift;

    return shifter(pawns & RightAttkingPawns, 8 + inc) |
           shifter(pawns & LeftAttkingPawns , 8 - inc);
}

Bitboard
BishopAttackSquares(Square __pos, Bitboard _Ap)
{    
    uint64_t magic = plt::BishopMagics[__pos];
    int      bits  = plt::BishopShifts[__pos];
    Bitboard mask  = plt::BishopMasks[__pos];
    uint64_t start = plt::BishopStartIndex[__pos];

    uint64_t occupancy = (magic * (_Ap & mask)) >> bits;
    return plt::BishopMovesLookUp[start + occupancy];
}

Bitboard
KnightAttackSquares(Square __pos, Bitboard _Ap)
{ return plt::KnightMasks[__pos] + (_Ap - _Ap); }

Bitboard
RookAttackSquares(Square __pos, Bitboard _Ap)
{
    uint64_t magic = plt::RookMagics[__pos];
    int      bits  = plt::RookShifts[__pos];
    Bitboard mask  = plt::RookMasks[__pos];
    uint64_t start = plt::RookStartIndex[__pos];

    uint64_t occupancy = (magic * (_Ap & mask)) >> bits;
    return plt::RookMovesLookUp[start + occupancy];
}

Bitboard
QueenAttackSquares(Square __pos, Bitboard _Ap)
{ return BishopAttackSquares(__pos, _Ap) ^ RookAttackSquares(__pos, _Ap); }



bool
InCheck(const ChessBoard& _cb)
{
    Color c_my  =  _cb.color;
    Color c_emy = ~_cb.color;

    Square k_sq = SquareNo( _cb.piece(c_my, KING) );
    Bitboard occupied = _cb.All();

    return (RookAttackSquares(k_sq, occupied) & (_cb.piece(c_emy, ROOK  ) | _cb.piece(c_emy, QUEEN)))
      or (BishopAttackSquares(k_sq, occupied) & (_cb.piece(c_emy, BISHOP) | _cb.piece(c_emy, QUEEN)))
      or (KnightAttackSquares(k_sq, occupied) &  _cb.piece(c_emy, KNIGHT))
      or (plt::PawnCaptureMasks[c_my][k_sq]   &  _cb.piece(c_emy, PAWN  ));
}


string
PrintMove(Move move, ChessBoard _cb)
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

    Square ip = Square(move & 63);
    Square fp = Square((move >> 6) & 63);

    int ip_col = ip & 7;
    int fp_col = fp & 7;

    int ip_row = (ip - ip_col) >> 3;
    int fp_row = (fp - fp_col) >> 3;

    PieceType  pt = PieceType((move >> 12) & 7);
    PieceType cpt = PieceType((move >> 15) & 7);

    Bitboard Apieces = _cb.All();

    _cb.MakeMove(move);
    string gives_check = InCheck(_cb) ? "+" : "";
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
        (pt == BISHOP ? BishopAttackSquares(fp, Apieces) :
        (pt == KNIGHT ? KnightAttackSquares(fp, Apieces) :
        (pt == ROOK   ?   RookAttackSquares(fp, Apieces) :
        QueenAttackSquares(fp, Apieces))));

    pieces = (pieces & _cb.piece(_cb.color, pt)) ^ (1ULL << ip);

    string piece_name = string(1, piece_names[pt - 2]);
    string end_part = captures + IndexToSquare(fp_row, fp_col) + gives_check;

    if (pieces == 0)
        return piece_name + end_part;
    
    bool row = true, col = true;

    while (pieces > 0)
    {
        Square __pos = NextSquare(pieces);

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
PrintMovelist(MoveList myMoves, ChessBoard _cb)
{
    using std::setw;

    // Sort the moves based on their priority
    for (size_t i = 0; i < myMoves.size(); i++) {
        for (size_t j = i + 1; j < myMoves.size(); j++) {
            if ((myMoves.pMoves[i] >> 21) < (myMoves.pMoves[j] >> 21))
                std::swap(myMoves.pMoves[i], myMoves.pMoves[j]);
        }
    }


    cout << "MoveCount : " << myMoves.size() << '\n'
        << " | No. |   Move   | Encode-Move | Priority |" << endl;

    for (size_t i = 0; i < myMoves.size(); i++)
    {
        Move move = myMoves.pMoves[i];
        string moveString = PrintMove(move, _cb);
        int priority = move >> 24;

        cout << " | " << setw(3)  << (i + 1)
             << " | " << setw(8)  << moveString
             << " | " << setw(11) << move
             << " | " << setw(8)  << priority
             << " |"  << endl;
    }

    cout << endl;
}


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
    bool gives_check = InCheck(pos);
    pos.UnmakeMove();
    return gives_check;
}
