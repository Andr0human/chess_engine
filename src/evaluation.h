
#ifndef EVALUATION_H
#define EVALUATION_H

#include "bitboard.h"
#include "PieceSquareTable.h"


class EvalData
{
  private:

  template <Color cMy>
  void materialCount(const ChessBoard& pos)
  {
    pieces[cMy] =
    pos.count<cMy, BISHOP>()
      + pos.count<cMy, KNIGHT>()
      + pos.count<cMy, ROOK  >()
      + pos.count<cMy, QUEEN >();
  }

	public:

  int pieces[COLOR_NB];

  int boardWeight;
  float phase;

  EvalData(const ChessBoard& pos)
  {
  	materialCount<WHITE>(pos);
  	materialCount<BLACK>(pos);

  	boardWeight = pos.boardWeight;
  	phase = float(boardWeight) / float(GamePhaseLimit);
  }

  bool
  noWhitePiecesOnBoard(const ChessBoard& pos) const noexcept
  { return pos.count<WHITE, PAWN>() + pieces[WHITE] == 0; }

  bool
  noBlackPiecesOnBoard(const ChessBoard& pos) const noexcept
  { return pos.count<BLACK, PAWN>() + pieces[BLACK] == 0; }
};

// Eval weights, split into midgame and endgame. Each weight scales one term
// before the midgame and endgame scores are blended. The tuner changes the one
// global instance between iterations. Mobility and threats are midgame-only and
// king distance is endgame-only, as in midGameScore and endGameScore. Mobility
// has one weight per piece type so the tuner can balance them.
struct EvalWeights
{
  float materialWeightMg      = 1.0f;
  float materialWeightEg      = 1.0f;
  float pieceTableWeightMg    = 1.2f;
  float pieceTableWeightEg    = 1.8f;
  float pawnStructureWeightEg = 0.7f;  // endgame-only (no midgame counterpart)
  float mobBishopWeightMg     = 8.0f;  // midgame-only
  float mobKnightWeightMg     = 9.5f;  // midgame-only
  float mobRookWeightMg       = 6.0f;  // midgame-only
  float mobQueenWeightMg      = 4.5f;  // midgame-only
  float threatsWeightMg       = 0.7f;  // midgame-only
  float distanceWeightEg      = 1.0f;  // endgame-only king-distance term
  float bishopPairWeightMg    =  40.0f;
  float bishopPairWeightEg    =  55.0f;  // pair worth more in open endgames
  float rookFileWeightMg      =  16.0f;  // per unit: open file ~32cp, semi-open ~16cp (midgame-only)
  float isolatedPawnWeightMg  = -16.0f;  // penalty; weight carries the sign
  float isolatedPawnWeightEg  =  -4.0f;  // eg isolani much milder than mg
};

extern EvalWeights evalWeights;

template <bool debug=false>
Score
threats(const ChessBoard& pos);

template <bool debug=false>
Score
evaluate(const ChessBoard& pos);


// Eval terms for one position, White minus Black. The tuner caches these so
// each iteration is plain arithmetic, with no board or move generation. The
// eval is linear in the weights, so evalFromComponents() gives exactly what
// evaluate<false>() gives (from White's side). `tunable` is false for the
// special endgames that skip the weighted eval (lone king, and king + bishop +
// pawn vs king). The tuner skips those.
struct EvalComponents
{
  bool  tunable = false;
  float phase   = 0.0f;
  // Midgame terms. Mobility is stored per piece type as raw popcount
  // differences.
  float matMg = 0.0f, ptMg = 0.0f, threats = 0.0f;
  float mobBishop = 0.0f, mobKnight = 0.0f, mobRook = 0.0f, mobQueen = 0.0f;
  // bishopPair (-1/0/+1) and isolated (White's isolated pawns minus Black's)
  // are used in both phases. rookFileMg is midgame-only.
  float bishopPair = 0.0f, rookFileMg = 0.0f, isolated = 0.0f;
  // Endgame terms. pawnEg is the endgame pawn structure score (passed pawns,
  // king support, safe promotion, doubled pawns). It has no midgame version.
  float matEg = 0.0f, ptEg = 0.0f, pawnEg = 0.0f, distance = 0.0f;
};

EvalComponents
extractEvalComponents(const ChessBoard& pos);

// The weighted midgame and endgame sums, before evaluate() truncates each to a Score
// and blends them by phase.
struct PhaseSums
{
  float mg = 0.0f;
  float eg = 0.0f;
};

PhaseSums
phaseSumsFromComponents(const EvalComponents& ec, const EvalWeights& w);

// Does exactly what evaluate() does, including truncating the midgame and
// endgame scores to int before the phase blend.
Score
evalFromComponents(const EvalComponents& ec, const EvalWeights& w);


#endif

