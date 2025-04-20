
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

	Score pawnStructureScore;
	Score threatScore;

	constexpr static float materialWeight = 1.0f;
	constexpr static float pieceTableWeight = 1.8f;
	constexpr static float threatsWeight = 0.5f;
	constexpr static float mobilityWeight = 1.2f;
	constexpr static float pawnSructureWeight = 0.4f;

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

template <bool debug=false>
Score
threats(const ChessBoard& pos);

template <bool debug=false>
Score
evaluate(const ChessBoard& pos);


#endif

