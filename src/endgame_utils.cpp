
#include "endgame_utils.h"
#include "attacks.h"


namespace eg
{
  bool
  kingBetweenQueens(const Square kingSq, const Bitboard queen1, const Bitboard queen2)
  {
    auto between = [&](Bitboard pos, Bitboard neg) {
      return ((pos & queen1) and (neg & queen2)) or
             ((pos & queen2) and (neg & queen1));
    };

    return between(plt::upMasks[kingSq],      plt::downMasks[kingSq])      or
           between(plt::leftMasks[kingSq],    plt::rightMasks[kingSq])     or
           between(plt::upLeftMasks[kingSq],  plt::downRightMasks[kingSq]) or
           between(plt::upRightMasks[kingSq], plt::downLeftMasks[kingSq]);
  }

  int
  getRuleOfSquareIndex(const EgBoard& pos, Color side, Square pawnSq)
  {
    const int sideAdvantage = int(side == pos.color);
    const int incFactor     = 2 * int(side) - 1;

    const Bitboard myKing = pos.getPiece(side, KING);
    const Bitboard   pawn = pos.getPiece(side, PAWN);

    int ruleOfSquareIndex = pawnSq;

    if (!sideAdvantage)
      ruleOfSquareIndex -= 8 * incFactor;

    if ((pawn & relativeRank[side][2]) and (myKing & ~plt::pawnMasks[side][pawnSq]))
      ruleOfSquareIndex += 8 * incFactor;

    return ruleOfSquareIndex;
  }

  int
  bishopCornerDistance(const Square k, const Bitboard bishop)
  {
    const Bitboard corners = CornerSquares & bishopColourSquares(bishop);

    int d = 8;
    for (Bitboard c = corners; c != 0; c &= c - 1)
      d = std::min(d, plt::chebyshevDistance(k, squareNo(c & -c)));
    return d;
  }

  template <PieceType... Men>
  Bitboard
  attackerCover(const EgBoard& pos, const Color atk)
  {
    const Bitboard thru = pos.all() ^ pos.getPiece(~atk, KING);
    return (attackSquares<KING>(squareNo(pos.getPiece(atk, KING)), 0) | ... |
            attackSquares<Men>(squareNo(pos.getPiece(atk, Men)), thru));
  }

  template Bitboard attackerCover<ROOK, BISHOP  >(const EgBoard& pos, Color atk);
  template Bitboard attackerCover<ROOK, KNIGHT  >(const EgBoard& pos, Color atk);
  template Bitboard attackerCover<BISHOP, KNIGHT>(const EgBoard& pos, Color atk);

  template <PieceType Slider>
  bool
  quietMoveHits(const Square s, const Bitboard occ, const Bitboard target, const Bitboard through)
  {
    const Bitboard rest = occ ^ (1ULL << s);
    for (Bitboard to = attackSquares<Slider>(s, occ) & ~occ; to != 0; to &= to - 1)
    {
      const Bitboard toB = to & -to;
      if (attackSquares<Slider>(squareNo(toB), (rest | toB) ^ through) & target)
        return true;
    }
    return false;
  }

  template bool quietMoveHits<BISHOP>(Square s, Bitboard occ, Bitboard target, Bitboard through);
  template bool quietMoveHits<ROOK  >(Square s, Bitboard occ, Bitboard target, Bitboard through);

  template <PieceType Minor>
  int
  rookSafeMoves(const EgBoard& pos, const Color atk)
  {
    const Color def = ~atk;

    const Bitboard dkB    = pos.getPiece(def, KING);
    const Bitboard drB    = pos.getPiece(def, ROOK);
    const Bitboard drLift = pos.all() ^ drB;

    const Bitboard akAtt = attackSquares<KING >(squareNo(pos.getPiece(atk, KING )), 0);
    const Bitboard arAtt = attackSquares<ROOK >(squareNo(pos.getPiece(atk, ROOK )), drLift);
    const Bitboard amAtt = attackSquares<Minor>(squareNo(pos.getPiece(atk, Minor)), drLift);
    const Bitboard dkAtt = attackSquares<KING >(squareNo(dkB), 0);
    const Bitboard drAtt = attackSquares<ROOK >(squareNo(drB), pos.all());

    return popCount(drAtt & ~dkB & ~amAtt & (~(akAtt | arAtt) | dkAtt));
  }

  template int rookSafeMoves<KNIGHT>(const EgBoard& pos, Color atk);
  template int rookSafeMoves<BISHOP>(const EgBoard& pos, Color atk);

  bool
  backRankThreat(const EgBoard& pos)
  {
    const Color def = pos.color, atk = ~def;

    const Bitboard akB = pos.getPiece(atk, KING);
    const Bitboard dkB = pos.getPiece(def, KING);
    const Square ak = squareNo(akB), dk = squareNo(dkB);
    if (plt::chebyshevDistance(ak, dk) != 2)
      return false;

    // The attacking rook and minor block the king's lines.
    const Bitboard aMen  = pos.getPiece(atk, ALL) ^ akB;
    const Bitboard drAtt = attackSquares<ROOK>(squareNo(pos.getPiece(def, ROOK)), pos.all());

    return (((akB & Rank18) and std::abs((ak >> 3) - (dk >> 3)) == 2) or
            ((akB & FileAH) and std::abs((ak  & 7) - (dk  & 7)) == 2))
        and (drAtt & ~dkB & attackSquares<ROOK>(ak, aMen));
  }

  template <PieceType Minor>
  bool
  rookMatesInOne(const EgBoard& pos)
  {
    const Color def = pos.color, atk = ~def;

    const Bitboard akB = pos.getPiece(atk, KING);
    const Square   ak  = squareNo(akB);
    if (edgeDistance(ak) != 0)
      return false;

    const Bitboard arB = pos.getPiece(atk, ROOK );
    const Bitboard amB = pos.getPiece(atk, Minor);
    const Bitboard dkB = pos.getPiece(def, KING );
    const Bitboard drB = pos.getPiece(def, ROOK );

    const Square ar = squareNo(arB), am = squareNo(amB);
    const Square dk = squareNo(dkB), dr = squareNo(drB);
    const Bitboard occ    = pos.all();
    const Bitboard drLift = occ ^ drB;

    const Bitboard akAtt = attackSquares<KING>(ak, 0);
    const Bitboard dkAtt = attackSquares<KING>(dk, 0);
    const Bitboard drAtt = attackSquares<ROOK>(dr, occ);

    for (Bitboard t = drAtt & attackSquares<ROOK>(ak, occ) & ~dkB; t != 0; t &= t - 1)
    {
      const Bitboard sB     = t & -t;
      const Square   s      = squareNo(sB);
      const Bitboard o      = drLift | sB;
      const Bitboard arLeft = arB & ~sB;
      const Bitboard amLeft = amB & ~sB;
      const Bitboard arO    = attackSquares<ROOK >(ar, o);
      const Bitboard amO    = attackSquares<Minor>(am, o);

      // A rook pinned to its king by the attacking rook or bishop can't move off
      // the line. (A knight hitting the king would be a check.)
      if ((arLeft and (arO & dkB)) or (amLeft and (amO & dkB)))
        continue;

      // Each king square is guarded by the rook (seen through the king,
      // 8/8/8/1r6/8/8/8/K1kN1R2 b) or the defending king, or holds the king's
      // own rook or minor (8/8/8/1r6/8/8/R7/KNk5 b). The king takes the checking
      // rook unless the defending king guards it.
      if (akAtt & ~dkAtt & ~attackSquares<ROOK>(s, o ^ akB) & ~(arLeft | amLeft))
        continue;

      // The rook or minor takes the checking rook or blocks the check.
      const Bitboard stop = sB | (attackSquares<ROOK>(s, o) & attackSquares<ROOK>(ak, o));
      if ((arLeft and (arO & stop)) or (amLeft and (amO & stop)))
        continue;

      return true;
    }

    return false;
  }

  template bool rookMatesInOne<KNIGHT>(const EgBoard& pos);
  template bool rookMatesInOne<BISHOP>(const EgBoard& pos);

  bool
  loneKingLoses(const Square k, const Bitboard aAtt, const Bitboard rookB)
  {
    const Bitboard m = attackSquares<KING>(k, 0) & ~aAtt;
    return not (m & rookB) and (m != 0 or (aAtt & (1ULL << k)));
  }

  bool
  knightForks(const EgBoard& pos, const Square k, const Square r, const Bitboard o)
  {
    const Color atk = pos.color, def = ~atk;

    const Bitboard akB = pos.getPiece(atk, KING  );
    const Bitboard anB = pos.getPiece(atk, KNIGHT);
    const Bitboard drB = pos.getPiece(def, ROOK  );
    const Square   dr  = squareNo(drB);
    const Bitboard kB  = 1ULL << k, rB = 1ULL << r;

    if ((kB & anB) or (attackSquares<ROOK>(dr, o ^ anB) & akB))
      return false;

    const Bitboard akAtt  = attackSquares<KING>(squareNo(akB), 0);
    const Bitboard aAfter = akAtt | attackSquares<KNIGHT>(dr, 0) | attackSquares<ROOK>(r, o ^ anB ^ kB);

    for (Bitboard t = knightForkSquares(squareNo(anB), k, dr, o); t; t &= t - 1)
    {
      const Bitboard sB  = t & -t;
      const Bitboard nS  = attackSquares<KNIGHT>(squareNo(sB), 0);
      const Bitboard esc = attackSquares<KING>(k, 0) & ~drB & ~(akAtt | nS | attackSquares<ROOK>(r, o ^ anB ^ sB ^ kB));

      bool holds = esc & rB;
      for (Bitboard u = esc & ~rB; u != 0 and not holds; u &= u - 1)
        holds = not loneKingLoses(squareNo(u & -u), aAfter, rB);
      if (not holds)
        return true;
    }

    return false;
  }

  template <PieceType Minor>
  bool
  rookSkewers(const EgBoard& pos, const Square r, const Square k, const Bitboard o)
  {
    const Color atk = pos.color, def = ~atk;

    const Bitboard akB = pos.getPiece(atk, KING );
    const Bitboard amB = pos.getPiece(atk, Minor);
    const Bitboard drB = pos.getPiece(def, ROOK );
    const Square   am  = squareNo(amB), dr = squareNo(drB);
    const Bitboard kB  = 1ULL << k;

    const Bitboard akAtt = attackSquares<KING>(squareNo(akB), 0);

    for (Bitboard t = attackSquares<ROOK>(r, o) & ~o & attackSquares<ROOK>(k, o); t; t &= t - 1)
    {
      const Bitboard sB   = t & -t;
      const Bitboard occS = o ^ (1ULL << r) ^ sB;
      const Bitboard thru = attackSquares<ROOK>(squareNo(sB), occS ^ kB);
      if (not (thru & drB) or (attackSquares<ROOK>(dr, occS) & (akB | sB)))
        continue;

      // The minor's squares once the king has stepped off k, and whether it
      // guards the rook on dr after RxR.
      const Bitboard amAtt  = attackSquares<Minor>(am, occS ^ kB);
      const bool     guards = (akAtt | attackSquares<Minor>(am, occS ^ kB ^ sB)) & drB;

      const Bitboard esc  = attackSquares<KING>(k, 0) & ~drB & ~(akAtt | amAtt | thru);
      Bitboard       back = guards ? 0 : esc & attackSquares<KING>(dr, 0);
      bool           lost = not (esc & sB);

      if constexpr (Minor == KNIGHT)
      {
        for (; back != 0 and lost; back &= back - 1)
          lost = knightForks(pos, squareNo(back & -back), squareNo(sB), occS ^ kB ^ (back & -back));
      }
      else
      {
        // A bishop move after that step can pin the rook to its king (attacked
        // twice, guarded once) or check the king away from it. Any bishop move
        // that hits the rook or the king counts, unless the king took the bishop.
        for (; back != 0 and lost; back &= back - 1)
        {
          const Bitboard eB   = back & -back;
          const Bitboard occE = occS ^ kB ^ eB;
          const Bitboard bTo  = attackSquares<BISHOP>(am, occE) & ~occE;
          lost = not (eB & amB)
             and (bTo & (attackSquares<BISHOP>(dr, occE) | attackSquares<BISHOP>(squareNo(eB), occE)));
        }
      }

      if (lost)
        return true;
    }

    return false;
  }

  template bool rookSkewers<KNIGHT>(const EgBoard& pos, Square r, Square k, Bitboard o);
  template bool rookSkewers<BISHOP>(const EgBoard& pos, Square r, Square k, Bitboard o);

  namespace
  {
    // How deep this thread is in a lookahead.
    thread_local int lookaheadDepth = 0;

    // Marks this thread as inside a lookahead for as long as it lives.
    struct Lookahead
    {
      Lookahead()  { ++lookaheadDepth; }
      ~Lookahead() { --lookaheadDepth; }
    };

    // The king of side c is in check.
    bool
    inCheck(const EgBoard& pos, const Color c)
    { return attackers(pos, squareNo(pos.getPiece(c, KING)), ~c); }

    // Calls f(from, to) for each move of side c (pawns left out, legal or not),
    // and returns true as soon as f does.
    template <typename F>
    bool
    anyMove(const EgBoard& pos, const Color c, F&& f)
    {
      const Bitboard occ = pos.all(), own = pos.getPiece(c, ALL);
      for (Bitboard p = own; p != 0; p &= p - 1)
      {
        const Bitboard sB = p & -p;
        const Square   s  = squareNo(sB);
        Bitboard to = (sB & pos.getPiece(c, KING  )) ? attackSquares<KING  >(s, 0)
                    : (sB & pos.getPiece(c, KNIGHT)) ? attackSquares<KNIGHT>(s, 0)
                    : (sB & pos.getPiece(c, BISHOP)) ? attackSquares<BISHOP>(s, occ)
                    : (sB & pos.getPiece(c, ROOK  )) ? attackSquares<ROOK  >(s, occ)
                    : (sB & pos.getPiece(c, QUEEN )) ? attackSquares<QUEEN >(s, occ) : 0;
        for (to &= ~own; to != 0; to &= to - 1)
          if (f(s, squareNo(to & -to)))
            return true;
      }
      return false;
    }
  }

  template <PieceType Man>
  bool
  forcingMoveDraws(const EgBoard& pos, const bool canWin, const bool quiet)
  {
    if (lookaheadDepth > 0)
      return false;
    const Lookahead inside;

    const Color    us     = pos.color, them = ~us;
    const Bitboard theirs = pos.getPiece(them, ALL);
    const bool     chk    = inCheck(pos, us);

    // Calls f on the position after each legal capture or check (each legal
    // move when in check, or when `all` is set), and returns true as soon as f
    // does.
    const auto anyForcing = [&] (auto&& f, const bool all = false) {
      return anyMove(pos, us, [&] (const Square from, const Square to) {
        const EgBoard next = pos.play(from, to);
        return not inCheck(next, us)
           and (all or chk or (theirs & (1ULL << to)) or inCheck(next, them))
           and f(next);
      });
    };

    // The other side has a reply that reaches a known draw or takes the man.
    const auto refuted = [&] (const EgBoard& next) {
      const Bitboard man = next.getPiece(us, Man);
      return anyMove(next, them, [&] (const Square from, const Square to) {
        const EgBoard reply = next.play(from, to);
        return not inCheck(reply, them) and ((man & (1ULL << to)) or isTheoreticalDraw(reply));
      });
    };

    return anyForcing([] (const EgBoard& next) { return isTheoreticalDraw(next); }, quiet)
       and not (canWin and anyForcing([&] (const EgBoard& next) {
         return not isTheoreticalDraw(next) and not refuted(next);
       }));
  }

  template bool forcingMoveDraws<KNIGHT>(const EgBoard& pos, bool canWin, bool quiet);
  template bool forcingMoveDraws<ROOK  >(const EgBoard& pos, bool canWin, bool quiet);

  bool
  checksRefuted(const EgBoard& pos)
  {
    if (lookaheadDepth > 0)
      return false;
    const Lookahead inside;

    const Color us = pos.color, them = ~us;
    return not anyMove(pos, us, [&] (const Square from, const Square to) {
      const EgBoard next = pos.play(from, to);
      return not inCheck(next, us) and inCheck(next, them)
         and not anyMove(next, them, [&] (const Square f, const Square t) {
           const EgBoard reply = next.play(f, t);
           return not inCheck(reply, them) and isTheoreticalDraw(reply);
         });
    });
  }

  bool
  everyMoveDraws(const EgBoard& pos)
  {
    if (lookaheadDepth > 0)
      return false;
    const Lookahead inside;

    const Color us  = pos.color;
    bool        any = false;
    const bool  won = anyMove(pos, us, [&] (const Square from, const Square to) {
      const EgBoard next = pos.play(from, to);
      if (inCheck(next, us))
        return false;
      any = true;
      return not isTheoreticalDraw(next);
    });
    return any and not won;
  }
}
