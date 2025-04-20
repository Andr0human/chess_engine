
#include "move_utils.h"
#include "attacks.h"

void
decodeMove(Move move)
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
printMove(Move move, ChessBoard pos)
{
  if (move == NULL_MOVE)
    return string("null");

  const auto indexToRow = [] (int row)
  { return string(1, char(row + 49)); };

  const auto indexToCol = [] (int col)
  { return string(1, char(col + 97)); };

  const auto indexToSquare = [&] (int row, int col)
  { return indexToCol(col) + indexToRow(row); };

  const char pieceNames[4] = {'B', 'N', 'R', 'Q'};

  Square ip = Square(move & 63);
  Square fp = Square((move >> 6) & 63);

  int ipCol = ip & 7;
  int fpCol = fp & 7;

  int ipRow = (ip - ipCol) >> 3;
  int fpRow = (fp - fpCol) >> 3;

  PieceType  pt = PieceType((move >> 12) & 7);
  PieceType cpt = PieceType((move >> 15) & 7);

  Bitboard apieces = pos.all();

  pos.makeMove(move);
  bool check = (((move >> 22) & 1) == WHITE)
    ? inCheck<BLACK>(pos) : inCheck<WHITE>(pos);
  string givesCheck = check ? "+" : "";
  pos.unmakeMove();

  string captures = (cpt != 0) ? "x" : "";

  if (pt == PAWN)
  {
    string pawnCaptures
      = (std::abs(ipCol - fpCol) == 1)
      ? (indexToCol(ipCol) + "x") : "";

    string destSquare = indexToSquare(fpRow, fpCol);

    // No promotion
    if (((1ULL << fp) & Rank18) == 0)
      return pawnCaptures + destSquare + givesCheck;

    int ppt = (move >> 18) & 3;
    string promotedPiece = string("=") + string(1, pieceNames[ppt]);

    return pawnCaptures + destSquare + promotedPiece + givesCheck;
  }

  if (pt == KING)
  {
    Bitboard fpos = 1ULL << fp;
    // Castling
    if (std::abs(ipCol - fpCol) == 2)
      return string((fpos & FileG) ? "O-O" : "O-O-O") + givesCheck;

    return string("K") + captures + indexToSquare(fpRow, fpCol) + givesCheck;
  }

  // If piece is [BISHOP, KNIGHT, ROOK, QUEEN]
  Bitboard pieces =
    (pt == BISHOP ? attackSquares<BISHOP>(fp, apieces) :
    (pt == KNIGHT ? attackSquares<KNIGHT>(fp, apieces) :
    (pt == ROOK   ? attackSquares< ROOK >(fp, apieces) :
    attackSquares<QUEEN>(fp, apieces))));

  pieces = (pieces & pos.getPiece(pos.color, pt)) ^ (1ULL << ip);

  string pieceName = string(1, pieceNames[pt - 2]);
  string endPart = captures + indexToSquare(fpRow, fpCol) + givesCheck;

  if (pieces == 0)
    return pieceName + endPart;

  bool row = true, col = true;

  while (pieces > 0)
  {
    Square __pos = nextSquare(pieces);

    int posCol = __pos & 7;
    int posRow = (__pos - posCol) >> 3;

    if (posCol == ipCol) col = false;
    if (posRow == ipRow) row = false;
  }

  if (col) return pieceName + indexToCol(ipCol) + endPart;
  if (row) return pieceName + indexToRow(ipRow) + endPart;

  return pieceName + indexToSquare(ipRow, ipCol) + endPart;
}

template <Color cMy>
bool
inCheck(const ChessBoard& pos)
{
  constexpr Color cEmy = ~cMy;

  Square kSq = squareNo( pos.piece<cMy, KING>() );
  Bitboard occupied = pos.all();

  return (attackSquares<ROOK>(  kSq, occupied) & (pos.piece<cEmy, ROOK  >() | pos.piece<cEmy, QUEEN>()))
      or (attackSquares<BISHOP>(kSq, occupied) & (pos.piece<cEmy, BISHOP>() | pos.piece<cEmy, QUEEN>()))
      or (attackSquares<KNIGHT>(kSq, occupied) &  pos.piece<cEmy, KNIGHT>())
      or (attackSquares<cMy, PAWN>(kSq)        &  pos.piece<cEmy, PAWN  >());
}

template bool
inCheck<WHITE>(const ChessBoard& pos);

template bool
inCheck<BLACK>(const ChessBoard& pos);
