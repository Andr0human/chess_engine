
#include "move_utils.h"
#include "attacks.h"


template <MoveType mt, bool checks, NextSquareFunc nextSquare>
void
MoveList::FillMoves(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr int typeBit = mt << 20;
  PieceType ipt = PieceType((baseMove >> 12) & 7);

  while (endSquares > 0)
  {
    Square fp = nextSquare(endSquares);
    Move move = baseMove | typeBit | (fp << 6);

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    if (checks and ((1ULL << fp) & squaresThatCheckEnemyKing[ipt - 1]))
      move |= 1 << 23;

    movesArray.add(move);
  }
}

template <MoveType mt, bool checks, NextSquareFunc nextSquare>
void
MoveList::FillKingMoves(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr int typeBit = mt << 20;
  const Square ip = from_sq(baseMove);

  while (endSquares > 0)
  {
    Square fp = nextSquare(endSquares);
    Move move = baseMove | typeBit | (fp << 6);

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    if (checks and ((1ULL << ip) & discoverCheckSquares) and ((1ULL << fp) & (~discoverCheckMasks[ip])))
      move |= 1 << 23;

    if (checks and (std::abs(fp - ip) == 2))
    {
      Bitboard rookBit =
        (fp > ip ? 32ULL : 8ULL) << (56 * (~color));

      if (rookBit & squaresThatCheckEnemyKing[ROOK - 1])
        move |= 1 << 23;
    }

    movesArray.add(move);
  }
}

template <bool checks, NextSquareFunc nextSquare>
void
MoveList::FillEnpassantPawns(const ChessBoard& pos, MoveArray& movesArray) const noexcept
{
  constexpr Move typeBit = CAPTURES << 20;
  const int colorBit = color << 22;
  Bitboard epPawns = enpassantPawns;
  Square fp = pos.EnPassantSquare();

  while (epPawns > 0)
  {
    Square ip = nextSquare(epPawns);
    Move move = typeBit | (colorBit) | (PAWN << 12) | (fp << 6) | ip;

    if (checks)
    {
      if ((1ULL << fp) & squaresThatCheckEnemyKing[0])
        move |= 1 << 23;

      Square emyKingSq  = SquareNo(pos.get_piece(~color, KING));
      Bitboard occupied = pos.All() ^ (1ULL << ip) ^ (1ULL << fp) ^ (1ULL << (fp - 8 * (2 * color - 1)));

      Bitboard attackBishop = AttackSquares<BISHOP>(emyKingSq, occupied);
      Bitboard attackRook   = AttackSquares<ROOK  >(emyKingSq, occupied);

      Bitboard rq = pos.get_piece(color, ROOK  ) | pos.get_piece(color, QUEEN);
      Bitboard bq = pos.get_piece(color, BISHOP) | pos.get_piece(color, QUEEN);

      if ((attackBishop & bq) or (attackRook & rq))
        move |= 1 << 23;
    }

    movesArray.add(move);
  }
}

template <MoveType mt, bool checks, NextSquareFunc nextSquare>
void
MoveList::FillShiftPawns(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  int shift
) const noexcept
{
  const int  colorBit = color << 22;
  const Move baseMove = (mt << 20) | colorBit | (PAWN << 12);

  while (endSquares > 0)
  {
    Square fp = nextSquare(endSquares);
    Square ip = fp + shift;

    Move move = baseMove | (fp << 6) | ip;

    if (checks
      and ((((1ULL << ip) & discoverCheckSquares) and ((1ULL << fp) & (~discoverCheckMasks[ip])))
       or ((1ULL << fp) & squaresThatCheckEnemyKing[0]))
    ) move |= 1 << 23;

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    movesArray.add(move);
  }
}

template <MoveType mt, bool checks, NextSquareFunc nextSquare>
void
MoveList::FillPawns(
  const ChessBoard& pos,
  MoveArray& movesArray,
  Bitboard endSquares,
  Move baseMove
) const noexcept
{
  constexpr int typeBit  = mt << 20;
  constexpr int checkBit = 1 << 23;
  const Square ip = from_sq(baseMove);

  while (endSquares > 0)
  {
    Square fp = nextSquare(endSquares);
    Move move = baseMove | typeBit | (fp << 6);
    Bitboard fPos = 1ULL << fp;

    if (mt == CAPTURES)
      move |= type_of(pos.PieceOnSquare(fp)) << 15;

    if (fPos & Rank18)
    {
      move |= PROMOTION << 20;
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
      ) move |= 1 << 23;

      movesArray.add(move);
    }
  }
}

size_t
MoveList::countMoves() const noexcept
{
  Bitboard pawnMask  = myPawns & initSquares;
  Bitboard mask = initSquares ^ pawnMask;

  size_t moveCount = 0;

  if (checkers < 2)
  {
    moveCount += PopCount(pawnDestSquares[0]) + PopCount(pawnDestSquares[1])
               + PopCount(pawnDestSquares[2]) + PopCount(pawnDestSquares[3])
               + PopCount(enpassantPawns);

    while (pawnMask)
    {
      Bitboard finalSquares = destSquares[NextSquare(pawnMask)];
      moveCount += 4 * PopCount(finalSquares & Rank18) + PopCount(finalSquares & ~Rank18);
    }
  }

  while (mask > 0)
    moveCount += PopCount(destSquares[NextSquare(mask)]);

  return moveCount;
}

template<bool captures, bool quiet, bool checks, NextSquareFunc nextSquare>
void
MoveList::getMovesImpl(const ChessBoard& pos, MoveArray& myMoves) const noexcept
{
  const int colorBit = color << 22;

  // fix pawns
  Bitboard emyPieces = pos.get_piece(~color, ALL);
  Bitboard kingMask  = pos.get_piece(color, KING) & initSquares;
  Bitboard pawnMask  = myPawns & initSquares;
  Bitboard pieceMask = initSquares ^ (pawnMask | kingMask);

  if (checkers < 2)
  {
    if (captures)
    {
      FillShiftPawns<CAPTURES, checks, nextSquare>(pos, myMoves, pawnDestSquares[0], 7 - 16 * color);
      FillShiftPawns<CAPTURES, checks, nextSquare>(pos, myMoves, pawnDestSquares[1], 9 - 16 * color);
    }
    if (quiet)
    {
      FillShiftPawns<NORMAL, checks, nextSquare>(pos, myMoves, pawnDestSquares[2], 16 - 32 * color);
      FillShiftPawns<NORMAL, checks, nextSquare>(pos, myMoves, pawnDestSquares[3],  8 - 16 * color);
    }

    while (pawnMask > 0)
    {
      Square ip = nextSquare(pawnMask);
      PieceType ipt = type_of(pos.PieceOnSquare(ip));
      Move baseMove = colorBit | (ipt << 12) | ip;

      Bitboard finalSquares = destSquares[ip];
      Bitboard  captSquares = finalSquares & emyPieces;
      Bitboard quietSquares = finalSquares ^ captSquares;

      if (captures)
        FillPawns<CAPTURES, checks, nextSquare>(pos, myMoves,  captSquares, baseMove);

      if (quiet)
        FillPawns<NORMAL  , checks, nextSquare>(pos, myMoves, quietSquares, baseMove);
    }
  }

  if (captures)
    FillEnpassantPawns<checks, nextSquare>(pos, myMoves);

  while (pieceMask > 0)
  {
    Square     ip = nextSquare(pieceMask);
    PieceType ipt = type_of(pos.PieceOnSquare(ip));
    Move baseMove = colorBit | (ipt << 12) | ip;

    if (checks and ((1ULL << ip) & discoverCheckSquares))
      baseMove |= 1 << 23;

    Bitboard finalSquares = destSquares[ip];
    Bitboard  captSquares = finalSquares & emyPieces;
    Bitboard quietSquares = finalSquares ^ captSquares;

    if (captures)
      FillMoves<CAPTURES, checks, nextSquare>(pos, myMoves, captSquares, baseMove);

    if (quiet)
      FillMoves<NORMAL  , checks, nextSquare>(pos, myMoves, quietSquares, baseMove);
  }

  if (kingMask)
  {
    Square     ip = SquareNo(kingMask);
    Move baseMove = colorBit | (KING << 12) | ip;

    Bitboard finalSquares = destSquares[ip];
    Bitboard  captSquares = finalSquares & emyPieces;
    Bitboard quietSquares = finalSquares ^ captSquares;

    if (captures)
      FillKingMoves<CAPTURES, checks, nextSquare>(pos, myMoves, captSquares, baseMove);

    if (quiet)
      FillKingMoves<NORMAL  , checks, nextSquare>(pos, myMoves, quietSquares, baseMove);
  }
}

template<bool captures, bool quiet, bool checks>
void
MoveList::getMoves(const ChessBoard& pos, MoveArray& myMoves) const noexcept
{
  if (pos.color == WHITE)
    getMovesImpl<captures, quiet, checks, NextSquare>(pos, myMoves);
  else
    getMovesImpl<captures, quiet, checks, NextSquareMsb>(pos, myMoves);
}

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

template void MoveList::getMoves<true , true , true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<true , false, true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, true , true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, false, true >(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<true , true , false>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<true , false, false>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, true , false>(const ChessBoard&, MoveArray&) const noexcept;
template void MoveList::getMoves<false, false, false>(const ChessBoard&, MoveArray&) const noexcept;
