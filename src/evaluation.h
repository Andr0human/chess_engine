
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

	int  pieces[COLOR_NB];

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

// Runtime-tunable evaluation blend weights (mg/eg split). Each weight scales a
// per-component subtotal before the midgame/endgame scores are blended. A single
// global instance is mutated in place by the Texel tuner between iterations (the
// engine is single-threaded). Mobility and threats are midgame-only and king-distance
// is endgame-only, matching how midGameScore/endGameScore use them.
//
// Mobility is split per piece type so the tuner can rebalance among bishop / knight /
// rook / queen. The knight default (9.0) absorbs the historical `2 *` weighting that
// used to live in mobilityStrength(); bishop / rook / queen default to 4.5 (the
// previous single mobilityWeightMg), so default eval is unchanged.
struct EvalWeights
{
	float materialWeightMg      = 1.0f;
	float materialWeightEg      = 1.0f;
	float pieceTableWeightMg    = 1.2f;
	float pieceTableWeightEg    = 1.8f;
	float pawnStructureWeightMg = 0.05f;
	float pawnStructureWeightEg = 0.7f;
	float mobBishopWeightMg     = 8.0f;  // midgame-only
	float mobKnightWeightMg     = 9.5f;  // midgame-only (absorbs the legacy 2x factor)
	float mobRookWeightMg       = 6.0f;  // midgame-only
	float mobQueenWeightMg      = 4.5f;  // midgame-only
	float threatsWeightMg       = 0.7f;  // midgame-only
	float distanceWeightEg      = 1.0f;  // endgame-only king-distance term
};

extern EvalWeights evalWeights;

template <bool debug=false>
Score
threats(const ChessBoard& pos);

template <bool debug=false>
Score
evaluate(const ChessBoard& pos);


// White-relative per-component eval subtotals for one position, cached by the Texel
// tuner so each iteration is pure arithmetic (no board/movegen). The blend is linear
// in the weights given these subtotals + phase, so evalFromComponents() reproduces
// evaluate<false>() (white-relative) exactly. `tunable` is false for special endgames
// (loneKing / bishopPawn) which bypass the weighted eval — those must be skipped.
struct EvalComponents
{
	bool  tunable = false;
	float phase   = 0.0f;
	// midgame components. Mobility is stored per piece type; raw popcount sums (no
	// 2x knight bake-in) so the tuner sees an unbiased per-piece subtotal.
	float matMg = 0.0f, ptMg = 0.0f, pawnMg = 0.0f, threats = 0.0f;
	float mobBishop = 0.0f, mobKnight = 0.0f, mobRook = 0.0f, mobQueen = 0.0f;
	// endgame components; pawnEg is the endgame pawn-structure subtotal
	// (passers / king-escort / safe-promote + doubled), distinct from the midgame
	// pawnMg (doubled-pawn penalty only)
	float matEg = 0.0f, ptEg = 0.0f, pawnEg = 0.0f, distance = 0.0f;
};

// Extract the white-relative component subtotals for a position.
EvalComponents
extractEvalComponents(const ChessBoard& pos);

// Reconstruct the white-relative static eval from cached components + weights.
// Mirrors evaluate()'s arithmetic exactly, including the int truncation of the mg/eg
// subscores before the phase blend.
Score
evalFromComponents(const EvalComponents& ec, const EvalWeights& w);


#endif

