
#ifndef ENDGAME_UTILS_H
#define ENDGAME_UTILS_H

#include "endgame.h"
#include "attacks.h"

// Helpers shared by the recognizers in endgame.cpp.
namespace eg
{
  // Ranks indexed relative to each side's home rank. Index 0 is unused.
  inline constexpr Bitboard relativeRank[COLOR_NB][9] = {
    { NoSquares, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1 },  // BLACK
    { NoSquares, Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8 },  // WHITE
  };

  inline constexpr Bitboard rank2to6[COLOR_NB] = {
    AllSquares ^ (Rank18 | Rank2),
    AllSquares ^ (Rank18 | Rank7)
  };

  inline constexpr Bitboard rank3to5[COLOR_NB] = {
    Rank4 | Rank5 | Rank6,
    Rank3 | Rank4 | Rank5
  };

  inline constexpr Bitboard rank4to7[COLOR_NB] = {
    Rank4 | Rank5 | Rank6 | Rank7,
    Rank2 | Rank3 | Rank4 | Rank5
  };

  // The one-liners below are defined here so the recognizers inline them. From
  // endgame_utils.cpp, LTO leaves them as calls.

  // How many squares `s` is from the nearest edge: 0 on the edge, 3 in the centre.
  inline int
  edgeDistance(const Square s)
  {
    const int r = s >> 3, f = s & 7;
    return std::min(std::min(r, 7 - r), std::min(f, 7 - f));
  }

  // A king in a corner with its own piece next to it. The piece blocks one of
  // the king's flight squares, so a minor apiece (KBKB, KNKN, KBKN) can be lost
  // here. Every won position in those three signatures has this shape.
  inline bool
  cornerBlockedByOwnPiece(const Bitboard king, const Bitboard piece)
  { return (king & CornerSquares) and (plt::chebyshevDistance(squareNo(king), squareNo(piece)) == 1); }

  // A slider on s hits `front`, and with front lifted it hits `back`. With back
  // behind front on the line, that is a pin, a battery, a skewer or a discovered
  // attack. It also holds when the slider hits back directly along another line.
  template <PieceType Slider>
  inline bool
  xrays(const Square s, const Bitboard occ, const Bitboard front, const Bitboard back)
  { return (attackSquares<Slider>(s, occ) & front) and (attackSquares<Slider>(s, occ ^ front) & back); }

  // Empty squares from which a knight on n hits both a and b: its fork squares.
  // Only a knight on a or b could take the knight there.
  inline Bitboard
  knightForkSquares(const Square n, const Square a, const Square b, const Bitboard occ)
  { return attackSquares<KNIGHT>(n, 0) & ~occ & attackSquares<KNIGHT>(a, 0) & attackSquares<KNIGHT>(b, 0); }

  // The squares of the bishop's colour, the only ones it can ever reach.
  inline Bitboard
  bishopColourSquares(const Bitboard bishop)
  { return (bishop & WhiteSquares) ? WhiteSquares : BlackSquares; }

  // The men of side c that hit square s, with the sliders' lines through the
  // board as it stands. Pawns are left out.
  inline Bitboard
  attackers(const EgBoard& pos, const Square s, const Color c)
  {
    const Bitboard occ = pos.all();
    const Bitboard qB  = pos.getPiece(c, QUEEN);
    return (attackSquares<KING  >(s, 0  ) & pos.getPiece(c, KING  ))
         | (attackSquares<KNIGHT>(s, 0  ) & pos.getPiece(c, KNIGHT))
         | (attackSquares<BISHOP>(s, occ) & (pos.getPiece(c, BISHOP) | qB))
         | (attackSquares<ROOK  >(s, occ) & (pos.getPiece(c, ROOK  ) | qB));
  }

  bool
  kingBetweenQueens(Square kingSq, Bitboard queen1, Bitboard queen2);

  int
  getRuleOfSquareIndex(const EgBoard& pos, Color side, Square pawnSq);

  // How many king steps k is from the nearer corner of the bishop's colour.
  int
  bishopCornerDistance(Square k, Bitboard bishop);

  // Squares the attacking king and `Men` cover around the defending king. The
  // sliders see through the king: it can't step back along the line it is
  // checked on.
  template <PieceType... Men>
  Bitboard
  attackerCover(const EgBoard& pos, Color atk);

  // A move of the slider on s to an empty square hits `target` from there, its
  // new lines seen through `through`. With a slider of its side behind it, that
  // is the second attack of a discovered check.
  template <PieceType Slider>
  bool
  quietMoveHits(Square s, Bitboard occ, Bitboard target, Bitboard through = 0);

  // KRB vs KR and KRN vs KR, either side to move. Defending rook moves to
  // squares where the rook is not lost: the minor doesn't hit it, and the
  // attacking king and rook either don't hit it or the defending king guards it
  // (RxR KxR leaves K and minor vs K).
  template <PieceType Minor>
  int
  rookSafeMoves(const EgBoard& pos, Color atk);

  // KRB vs KR and KRN vs KR. The defender (the lone rook's side) is to move and
  // not in check; the attacker has a rook and a `Minor`.

  // A back-rank threat: the kings are two apart, the attacking king is on the
  // edge with the defending king two lines in front of it, and the defending
  // rook reaches one of the attacking king's lines.
  bool
  backRankThreat(const EgBoard& pos);

  // The defending rook mates in one: a rook move checks the attacking king, the
  // king has no square to go to, and the rook or minor can neither take the
  // checking rook nor block the check. Only a king on the edge can be mated by
  // a rook.
  template <PieceType Minor>
  bool
  rookMatesInOne(const EgBoard& pos);

  // KRB vs KR and KRN vs KR with the attacker to move. These take the attacking
  // rook's square r, the defending king's square k and the occupancy o, so a
  // caller can ask about the position after a move or two.

  // The rook on r skewers the king on k: it checks from s with the defending
  // rook behind the king, so the rook can't take on s or block, and the move
  // must not expose the attacking king. The king must step off the line and
  // RxR wins, unless the king can take on s or step next to its unguarded rook
  // and take back. That step still loses to a knight fork, or to a bishop move
  // that hits the rook or the king.
  template <PieceType Minor>
  bool
  rookSkewers(const EgBoard& pos, Square r, Square k, Bitboard o);

  // KRN vs KR. The knight forks the king on k and the defending rook, with the
  // attacking rook on r. Nothing can take it on the fork square, a knight's
  // move from both. It can't move when pinned or when its king is in check, or
  // after the king took it. The king must step away, and the defender holds if
  // it takes the rook, or if after NxR it can take the rook or is stalemated.
  bool
  knightForks(const EgBoard& pos, Square k, Square r, Bitboard o);

  // After the attacker takes the defending rook, the lone king on k loses unless
  // it can take the rook on `rookB` or is stalemated. `aAtt` is every square the
  // attacker then hits, with the rook's lines seen through the king.
  bool
  loneKingLoses(Square k, Bitboard aAtt, Bitboard rookB);

  // One-move lookaheads that ask isTheoreticalDraw about the positions after
  // each move. A recognizer asked from inside one answers from its other rules
  // only, so lookaheads don't nest: each answers false when asked from inside
  // another. Pawnless material only.

  // The side to move holds when one of its captures or checks (any move when it
  // is in check) reaches a position the recognizers call drawn. That is enough
  // when it can't win. When it can (`canWin`), none of the other forcing moves
  // may win either: each must reach a known draw, or the other side must have a
  // reply that does or that takes `Man`, the side to move's one man besides its
  // king. A mate has no reply, so it counts as a win. Wins by a quiet move are
  // the caller's to rule out.
  template <PieceType Man>
  bool
  forcingMoveDraws(const EgBoard& pos, bool canWin);

  // No check by the side to move wins: each has a reply that reaches a position
  // the recognizers call drawn. A mate has no reply, so it counts as a win.
  // Captures and quiet moves are the caller's to rule out.
  bool
  checksRefuted(const EgBoard& pos);

  // Every legal move of the side to move reaches a position the recognizers call
  // drawn. With no legal move (mate or stalemate) it answers false.
  bool
  everyMoveDraws(const EgBoard& pos);
}

#endif
