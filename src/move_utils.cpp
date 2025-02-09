
#include "move_utils.h"
#include "attacks.h"


template <MoveType mt>
void
MoveList::FillMoves(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr int typeBit = mt << 21;

  while (endSquares > 0)
  {
    Square fp = NextSquare(endSquares);
    Move move = baseMove | typeBit | (fp << 6);

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;
    
    movesArray.add(move);
  }
}

void
MoveList::FillEnpassantPawns(MoveArray& movesArray, Square fp) const noexcept
{
  constexpr Move typeBit = CAPTURES << 21;
  const int colorBit = color << 20;
  Bitboard epPawns = enpassantPawns;

  while (epPawns > 0)
  {
    Square ip = NextSquare(epPawns);

    Move move = typeBit | (colorBit) | (PAWN << 12) | (fp << 6) | ip;
    movesArray.add(move);
  }
}

template <MoveType mt>
void
MoveList::FillShiftPawns(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  int shift
) const noexcept
{
  const int  colorBit = color << 20;
  const Move baseMove = (mt << 21) | colorBit | (PAWN << 12);

  while (endSquares > 0)
  {
    Square fp = NextSquare(endSquares);
    Square ip = fp + shift;

    Move move = baseMove | (fp << 6) | ip;

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    movesArray.add(move);

    if (((1ULL << fp) & Rank18))
    {
      movesArray.add(move | 0xC0000);
      movesArray.add(move | 0x80000);
      movesArray.add(move | 0x40000);
    }
  }
}

template <MoveType mt>
void
MoveList::FillPawns(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr int typeBit = mt << 21;
  while (endSquares > 0)
  {
    Square fp = NextSquare(endSquares);
    Move move = baseMove | typeBit | (fp << 6);

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    movesArray.add(move);

    if ((1ULL << fp) & Rank18)
    {
      movesArray.add(move | 0xC0000);
      movesArray.add(move | 0x80000);
      movesArray.add(move | 0x40000);
    }
  }
}

template<bool captures, bool quiet>
void
MoveList::getMoves(const ChessBoard& pos, MoveArray& myMoves) const noexcept
{
  const int colorBit = color << 20;

  // fix pawns
  Bitboard emyPieces = pos.get_piece(~color, ALL);
  Bitboard myPawns   = pos.get_piece(color, PAWN);
  Bitboard pawnMask  = myPawns & initSquares;
  Bitboard pieceMask = initSquares ^ pawnMask;

  if (checkers < 2)
  {
    if (captures)
    {
      FillShiftPawns<CAPTURES>(pos, myMoves, pawnDestSquares[0], 7 - 16 * color);
      FillShiftPawns<CAPTURES>(pos, myMoves, pawnDestSquares[1], 9 - 16 * color);
    }
    if (quiet)
    {
      FillShiftPawns<NORMAL>(pos, myMoves, pawnDestSquares[2], 16 - 32 * color);
      FillShiftPawns<NORMAL>(pos, myMoves, pawnDestSquares[3],  8 - 16 * color);
    }

    while (pawnMask > 0)
    {
      Square ip = NextSquare(pawnMask);
      PieceType ipt = type_of(pos.PieceOnSquare(ip));
      Move baseMove = colorBit | (ipt << 12) | ip;

      Bitboard finalSquares = destSquares[ip];
      Bitboard  captSquares = finalSquares & emyPieces;
      Bitboard quietSquares = finalSquares ^ captSquares;

      if (captures)
        FillPawns<CAPTURES>(pos, myMoves,  captSquares, baseMove);

      if (quiet)
        FillPawns<NORMAL  >(pos, myMoves, quietSquares, baseMove);
    }
  }

  if (captures)
    FillEnpassantPawns(myMoves, pos.EnPassantSquare());

  while (pieceMask > 0)
  {
    Square     ip = NextSquare(pieceMask);
    PieceType ipt = type_of(pos.PieceOnSquare(ip));
    Move baseMove = colorBit | (ipt << 12) | ip;

    Bitboard finalSquares = destSquares[ip];
    Bitboard  captSquares = finalSquares & emyPieces;
    Bitboard quietSquares = finalSquares ^ captSquares;

    if (captures)
      FillMoves<CAPTURES>(pos, myMoves, captSquares, baseMove);

    if (quiet)
      FillMoves<NORMAL  >(pos, myMoves, quietSquares, baseMove);
  }
}

void
DecodeMove(Move move)
{
  Square ip = Square(move & 63);
  Square fp = Square((move >> 6) & 63);

  PieceType pt  = PieceType((move >> 12) & 7);
  PieceType cpt = PieceType((move >> 15) & 7);
  Color cl      = Color((move >> 20) & 1);

  int ppt          = (move >> 18) & 3;
  int moveType     = (move >> 21) & 3;
  int givesCheck   = (move >> 23) & 1;
  int moveStrength = move >> 24;

  cout << "startSquare : " << ip << '\n';
  cout << "endSquare : " << fp << '\n';
  cout << "pieceType : " << pt << '\n';
  cout << "captPieceType : " << cpt << '\n';
  cout << "pieceColor : " << (cl == 1 ? "White\n" : "Black\n");
  cout << "moveType: " << moveType << '\n';
  cout << "moveStrength : " << moveStrength << '\n';
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
  bool check = (((move >> 20) & 1) == WHITE)
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

template void MoveList::FillMoves<CAPTURES>(const ChessBoard&, MoveArray&, Bitboard, Move) const noexcept;
template void MoveList::FillMoves<NORMAL>(const ChessBoard&, MoveArray&, Bitboard, Move) const noexcept;
template void MoveList::FillShiftPawns<CAPTURES>(const ChessBoard&, MoveArray&, Bitboard, int) const noexcept;
template void MoveList::FillShiftPawns<NORMAL>(const ChessBoard&, MoveArray&, Bitboard, int) const noexcept;
template void MoveList::FillPawns<CAPTURES>(const ChessBoard&, MoveArray&, Bitboard, Move) const noexcept;
template void MoveList::FillPawns<NORMAL>(const ChessBoard&, MoveArray&, Bitboard, Move) const noexcept;
template void MoveList::getMoves<true, true>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<true, false>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, true>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, false>(const ChessBoard&, MoveArray&) const noexcept;
