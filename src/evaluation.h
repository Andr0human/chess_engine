
#ifndef EVALUATION_H
#define EVALUATION_H

#include "bitboard.h"
#include "PieceSquareTable.h"


class EvalData
{
	private:

	template <Color c_my>
	void MaterialCount(const ChessBoard& pos)
	{
		pieces[c_my]  = pos.count<c_my, BISHOP>()
					  + pos.count<c_my, KNIGHT>()
					  + pos.count<c_my, ROOK  >()
					  + pos.count<c_my, QUEEN >();
	}


	public:

	// int w_pawns, w_knights, w_bishops, w_rooks, w_queens, w_pieces;
	// int b_pawns, b_knights, b_bishops, b_rooks, b_queens, b_pieces;

	int  pieces[COLOR_NB];

	int pieceCount;
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
		MaterialCount<WHITE>(pos);
		MaterialCount<BLACK>(pos);
		pieceCount = pieces[WHITE] + pieces[BLACK];

		phase = float(
			10 * (pos.count<WHITE, PAWN  >() + pos.count<BLACK, PAWN  >())
		  + 30 * (pos.count<WHITE, BISHOP>() + pos.count<BLACK, BISHOP>())
		  + 30 * (pos.count<WHITE, KNIGHT>() + pos.count<BLACK, KNIGHT>())
		  + 55 * (pos.count<WHITE, ROOK  >() + pos.count<BLACK, ROOK  >())
		  + 90 * (pos.count<WHITE, QUEEN >() + pos.count<BLACK, QUEEN >())
		) / float(GamePhaseLimit);

		boardWeight =
			PawnValueMg   * (pos.count<WHITE, PAWN  >() + pos.count<BLACK, PAWN  >())
		  + BishopValueMg * (pos.count<WHITE, BISHOP>() + pos.count<BLACK, BISHOP>())
		  + KnightValueMg * (pos.count<WHITE, KNIGHT>() + pos.count<BLACK, KNIGHT>())
		  + RookValueMg   * (pos.count<WHITE, ROOK  >() + pos.count<BLACK, ROOK  >())
		  + QueenValueMg  * (pos.count<WHITE, QUEEN >() + pos.count<BLACK, QUEEN >());
		
		// phase = float(boardWeight) / float(GamePhaseLimit);
	}

	bool
	NoWhitePiecesOnBoard(const ChessBoard& pos) const noexcept
	{ return pos.count<WHITE, PAWN>() + pieces[WHITE] == 0; }

	bool
	NoBlackPiecesOnBoard(const ChessBoard& pos) const noexcept
	{ return pos.count<BLACK, PAWN>() + pieces[BLACK] == 0; }
};

bool
isTheoreticalDraw(const ChessBoard& pos);

Score Evaluate(const ChessBoard& pos);

Score EvalDump(const ChessBoard& pos);

Score
EvaluateThreats(const ChessBoard& pos);


#endif

