
#include "move_utils.h"
#include "attacks.h"


template <MoveType mt, bool checks>
void
MoveList::FillMoves(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr int typeBit = mt << 21;
  PieceType ipt = PieceType((baseMove >> 12) & 7);

  while (endSquares > 0)
  {
    Square fp = NextSquare(endSquares);
    Move move = baseMove | typeBit | (fp << 6);

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    if (checks and ((1ULL << fp) & squaresThatCheckEnemyKing[ipt - 1]))
      move |= CHECK << 21;

    movesArray.add(move);
  }
}

template <MoveType mt, bool checks>
void
MoveList::FillKingMoves(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr int typeBit = mt << 21;
  const Square ip = from_sq(baseMove);

  while (endSquares > 0)
  {
    Square fp = NextSquare(endSquares);
    Move move = baseMove | typeBit | (fp << 6);

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    if (checks and ((1ULL << ip) & discoverCheckSquares) and ((1ULL << fp) & (~discoverCheckMasks[ip])))
      move |= CHECK << 21;

    if (checks and (std::abs(fp - ip) == 2))
    {
      Bitboard rookBit =
        (fp > ip ? 32ULL : 8ULL) << (56 * (~color));

      if (rookBit & squaresThatCheckEnemyKing[ROOK - 1])
        move |= CHECK << 21;
    }

    movesArray.add(move);
  }
}

template <bool checks>
void
MoveList::FillEnpassantPawns(const ChessBoard& pos, MoveArray& movesArray) const noexcept
{
  constexpr Move typeBit = CAPTURES << 21;
  const int colorBit = color << 20;
  Bitboard epPawns = enpassantPawns;
  Square fp = pos.EnPassantSquare();

  while (epPawns > 0)
  {
    Square ip = NextSquare(epPawns);
    Move move = typeBit | (colorBit) | (PAWN << 12) | (fp << 6) | ip;

    if (checks)
    {
      if ((1ULL << fp) & squaresThatCheckEnemyKing[0])
        move |= CHECK << 21;

      Square emyKingSq  = SquareNo(pos.get_piece(~color, KING));
      Bitboard occupied = pos.All() ^ (1ULL << ip) ^ (1ULL << fp) ^ (1ULL << (fp - 8 * (2 * color - 1)));

      Bitboard attackBishop = AttackSquares<BISHOP>(emyKingSq, occupied);
      Bitboard attackRook   = AttackSquares<ROOK  >(emyKingSq, occupied);

      Bitboard rq = pos.get_piece(color, ROOK  ) | pos.get_piece(color, QUEEN);
      Bitboard bq = pos.get_piece(color, BISHOP) | pos.get_piece(color, QUEEN);

      if ((attackBishop & bq) or (attackRook & rq))
        move |= CHECK << 21;
    }

    movesArray.add(move);
  }
}

template <MoveType mt, bool checks>
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

    if (checks
      and ((((1ULL << ip) & discoverCheckSquares) and ((1ULL << fp) & (~discoverCheckMasks[ip])))
       or ((1ULL << fp) & squaresThatCheckEnemyKing[0]))
    ) move |= CHECK << 21;

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    movesArray.add(move);
  }
}
#include "types.h"

template <MoveType mt, bool checks>
void
MoveList::FillPawns(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr int typeBit  = mt << 21;
  constexpr int checkBit = CHECK << 21;
  const Square ip = from_sq(baseMove);

  while (endSquares > 0)
  {
    Square fp = NextSquare(endSquares);
    Move move = baseMove | typeBit | (fp << 6);
    Bitboard fPos = 1ULL << fp;

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    if (fPos & Rank18)
    {
      move = move & ((1 << 21) - 1);
      move |= PROMOTION << 21;
      Move moveQ = move | 0xC0000;
      Move moveR = move | 0x80000;
      Move moveN = move | 0x40000;
      Move moveB = move;

      if (checks)
      {
        if (((1ULL << ip) & discoverCheckSquares) and (fPos & (~discoverCheckMasks[ip])))
        {
          moveQ |= checkBit;
          moveR |= checkBit;
          moveN |= checkBit;
          moveB |= checkBit;
        }
        else
        {
          Bitboard emyKing = pos.get_piece(~color, KING);
          Bitboard occupied = pos.All() ^ (1ULL << ip);

          Bitboard attackSqBishop = AttackSquares<BISHOP>(fp, occupied);
          Bitboard attackSqRook   = AttackSquares<ROOK  >(fp, occupied);
          Bitboard attackSqQueen  = attackSqBishop | attackSqRook;

          if (attackSqQueen  & emyKing) moveQ |= checkBit;
          if (attackSqRook   & emyKing) moveR |= checkBit;
          if (attackSqBishop & emyKing) moveB |= checkBit;
          if (fPos & squaresThatCheckEnemyKing[KNIGHT - 1]) moveN |= checkBit;
        }
      }

      movesArray.add(moveQ);
      movesArray.add(moveR);
      movesArray.add(moveN);
      movesArray.add(moveB);
    }
    else
    {
      if (checks
        and ((((1ULL << ip) & discoverCheckSquares) and (fPos & (~discoverCheckMasks[ip])))
         or (fPos & squaresThatCheckEnemyKing[0]))
      ) move |= CHECK << 21;

      movesArray.add(move);
    }
  }
}

template<bool captures, bool quiet, bool checks>
void
MoveList::getMoves(const ChessBoard& pos, MoveArray& myMoves) const noexcept
{
  const int colorBit = color << 20;

  // fix pawns
  Bitboard emyPieces = pos.get_piece(~color, ALL);
  Bitboard myPawns   = pos.get_piece(color, PAWN);
  Bitboard kingMask  = pos.get_piece(color, KING) & initSquares;
  Bitboard pawnMask  = myPawns & initSquares;
  Bitboard pieceMask = initSquares ^ (pawnMask | kingMask);

  if (checkers < 2)
  {
    if (captures)
    {
      FillShiftPawns<CAPTURES, checks>(pos, myMoves, pawnDestSquares[0], 7 - 16 * color);
      FillShiftPawns<CAPTURES, checks>(pos, myMoves, pawnDestSquares[1], 9 - 16 * color);
    }
    if (quiet)
    {
      FillShiftPawns<NORMAL, checks>(pos, myMoves, pawnDestSquares[2], 16 - 32 * color);
      FillShiftPawns<NORMAL, checks>(pos, myMoves, pawnDestSquares[3],  8 - 16 * color);
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
        FillPawns<CAPTURES, checks>(pos, myMoves,  captSquares, baseMove);

      if (quiet)
        FillPawns<NORMAL  , checks>(pos, myMoves, quietSquares, baseMove);
    }
  }

  if (captures)
    FillEnpassantPawns<checks>(pos, myMoves);

  while (pieceMask > 0)
  {
    Square     ip = NextSquare(pieceMask);
    PieceType ipt = type_of(pos.PieceOnSquare(ip));
    Move baseMove = colorBit | (ipt << 12) | ip;

    if (checks and ((1ULL << ip) & discoverCheckSquares))
      baseMove |= CHECK << 21;

    Bitboard finalSquares = destSquares[ip];
    Bitboard  captSquares = finalSquares & emyPieces;
    Bitboard quietSquares = finalSquares ^ captSquares;

    if (captures)
      FillMoves<CAPTURES, checks>(pos, myMoves, captSquares, baseMove);

    if (quiet)
      FillMoves<NORMAL  , checks>(pos, myMoves, quietSquares, baseMove);
  }

  if (kingMask)
  {
    Square     ip = SquareNo(kingMask);
    Move baseMove = colorBit | (KING << 12) | ip;

    Bitboard finalSquares = destSquares[ip];
    Bitboard  captSquares = finalSquares & emyPieces;
    Bitboard quietSquares = finalSquares ^ captSquares;

    if (captures)
      FillKingMoves<CAPTURES, checks>(pos, myMoves, captSquares, baseMove);

    if (quiet)
      FillKingMoves<NORMAL  , checks>(pos, myMoves, quietSquares, baseMove);
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

template void MoveList::getMoves<true , true , true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<true , false, true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, true , true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, false, true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<true , true , false>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<true , false, false>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, true , false>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, false, false>(const ChessBoard&, MoveArray&) const noexcept;
