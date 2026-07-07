
#ifndef ENDGAME_PROBE_H
#define ENDGAME_PROBE_H

#include "bitboard.h"

struct EndgameProbe
{
  bool signatureHit;
  bool positionHit;
  bool scoreCutsBeta;
  Score directionalScore;
};


EndgameProbe probeEndgame(const ChessBoard& pos);

#endif


/* implementation in single_thread.cpp

# old
if (!myMoves.exists<MType::CAPTURES>(pos) and isTheoreticalDraw(pos))
    return VALUE_DRAW;

# with endgame_probe
if (!myMoves.exists<MType::CAPTURES>(pos))
{
  EndgameProbe egp = probeEndgame(pos);

  if (egp.signatureHit)
  {
    if (egp.positionHit)
      return VALUE_DRAW;

    # implementation-1
    if (egp.directionalScore != 0 and egp.directionalScore >= beta)
      return egp.directionalScore;

    # implementation-2
    if (egp.directionalScore != 0 and egp.directionalScore > alpha)
      alpha = egp.directionalScore;
  }
}



*/