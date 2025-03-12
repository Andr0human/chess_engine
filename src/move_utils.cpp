
#include "move_utils.h"
#include "attacks.h"

void
DecodeMove(Move move)
{
  Square ip = Square(move & 63);
  Square fp = Square((move >> 6) & 63);

  PieceType pt  = PieceType((move >> 12) & 7);
  PieceType cpt = PieceType((move >> 15) & 7);
  Color cl      = Color((move >> 22) & 1);

  int ppt          = (move >> 18) & 3;
  int moveType     = (move >> 20) & 3;
  int givesCheck   = (move >> 23) & 1;

  cout << "startSquare : " << ip << '\n';
  cout << "endSquare : " << fp << '\n';
  cout << "pieceType : " << pt << '\n';
  cout << "captPieceType : " << cpt << '\n';
  cout << "pieceColor : " << (cl == 1 ? "White\n" : "Black\n");
  cout << "moveType: " << moveType << '\n';
  cout << "givesCheck: " << givesCheck << '\n';

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
  bool check = (((move >> 22) & 1) == WHITE)
    ? InCheck<BLACK>(_cb) : InCheck<WHITE>(_cb);
  string gives_check = check ? "+" : "";
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
    (pt == BISHOP ? AttackSquares<BISHOP>(fp, Apieces) :
    (pt == KNIGHT ? AttackSquares<KNIGHT>(fp, Apieces) :
    (pt == ROOK   ? AttackSquares< ROOK >(fp, Apieces) :
    AttackSquares<QUEEN>(fp, Apieces))));

  pieces = (pieces & _cb.get_piece(_cb.color, pt)) ^ (1ULL << ip);

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

template <Color c_my>
bool
InCheck(const ChessBoard& _cb)
{
  constexpr Color c_emy = ~c_my;

  Square k_sq = SquareNo( _cb.piece<c_my, KING>() );
  Bitboard occupied = _cb.All();

  return (AttackSquares<ROOK>(k_sq, occupied) & (_cb.piece<c_emy, ROOK  >() | _cb.piece<c_emy, QUEEN>()))
    or (AttackSquares<BISHOP>(k_sq, occupied) & (_cb.piece<c_emy, BISHOP>() | _cb.piece<c_emy, QUEEN>()))
    or (AttackSquares<KNIGHT>(k_sq, occupied) &  _cb.piece<c_emy, KNIGHT>())
    or (AttackSquares<c_my, PAWN>(k_sq)       &  _cb.piece<c_emy, PAWN  >());
}

template bool InCheck<WHITE>(const ChessBoard& pos);
template bool InCheck<BLACK>(const ChessBoard& pos);
