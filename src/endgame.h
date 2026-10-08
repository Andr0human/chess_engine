

#ifndef ENDGAMES_H
#define ENDGAMES_H

#include "bitboard.h"
#include "lookup_table.h"


// The part of a position the recognizers read: where each man stands and who is
// to move. Small enough to copy, so a recognizer can play a capture on a copy and
// ask about the position that results.
struct EgBoard
{
  array<Bitboard, 16> pieceBb{};
  Color color;

  explicit EgBoard(const ChessBoard& pos) : color(pos.color)
  {
    for (Color c : { WHITE, BLACK })
      for (PieceType pt : { PAWN, BISHOP, KNIGHT, ROOK, QUEEN, KING, ALL })
        pieceBb[make_piece(c, pt)] = pos.getPiece(c, pt);
  }

  Bitboard
  getPiece(Color c, PieceType pt) const noexcept
  { return pieceBb[make_piece(c, pt)]; }

  template <Color c, PieceType pt>
  Bitboard
  piece() const noexcept
  { return getPiece(c, pt); }

  // As in ChessBoard, the ALL count leaves out the king (the ALL bitboard keeps it).
  template <Color c, PieceType pt>
  int
  count() const noexcept
  { return popCount(getPiece(c, pt)) - (pt == ALL); }

  template <PieceType pt>
  int
  count() const noexcept
  { return count<WHITE, pt>() + count<BLACK, pt>(); }

  Bitboard
  all() const noexcept
  { return getPiece(WHITE, ALL) | getPiece(BLACK, ALL); }

  // The man on `from` moves to `to`, taking whatever stands there, and the other
  // side is to move. The caller checks legality; no castling, en passant or promotion.
  EgBoard
  play(Square from, Square to) const noexcept
  {
    EgBoard next = *this;
    for (Bitboard& bb : next.pieceBb)
    {
      if (bb & (1ULL << to))   bb ^= 1ULL << to;
      if (bb & (1ULL << from)) bb ^= (1ULL << from) | (1ULL << to);
    }
    next.color = ~color;
    return next;
  }
};


bool
isTheoreticalDraw(const ChessBoard& pos);


#endif
