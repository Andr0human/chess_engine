
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
		pawns[c_my]   = PopCount(pos.piece<c_my, PAWN  >());
		bishops[c_my] = PopCount(pos.piece<c_my, BISHOP>());
		knights[c_my] = PopCount(pos.piece<c_my, KNIGHT>());
		rooks[c_my]   = PopCount(pos.piece<c_my, ROOK  >());
		queens[c_my]  = PopCount(pos.piece<c_my, QUEEN >());
		pieces[c_my]  = bishops[c_my] + knights[c_my] + rooks[c_my] + queens[c_my];
	}


	public:

	// int w_pawns, w_knights, w_bishops, w_rooks, w_queens, w_pieces;
	// int b_pawns, b_knights, b_bishops, b_rooks, b_queens, b_pieces;

	int   pawns[COLOR_NB];
	int bishops[COLOR_NB];
	int knights[COLOR_NB];
	int   rooks[COLOR_NB];
	int  queens[COLOR_NB];
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
			10 * (pawns[WHITE] + pawns[BLACK])
		  + 30 * (bishops[WHITE] + bishops[BLACK] + knights[WHITE] + knights[BLACK])
		  + 55 * (rooks[WHITE] + rooks[BLACK])
		  + 90 * (queens[WHITE] + queens[BLACK]) ) / float(GamePhaseLimit);

		boardWeight =
			PawnValueMg   * (  pawns[WHITE] +   pawns[BLACK])
		  + BishopValueMg * (bishops[WHITE] + bishops[BLACK])
		  + KnightValueMg * (knights[WHITE] + knights[BLACK])
		  + RookValueMg   * (  rooks[WHITE] +   rooks[BLACK])
		  + QueenValueMg  * ( queens[WHITE] +  queens[BLACK]);
		
		// phase = float(boardWeight) / float(GamePhaseLimit);
	}

	constexpr bool
	NoWhitePiecesOnBoard() const noexcept
	{ return pawns[WHITE] + pieces[WHITE] == 0; }

	constexpr bool
	NoBlackPiecesOnBoard() const noexcept
	{ return pawns[BLACK] + pieces[BLACK] == 0; }
};


Score Evaluate(const ChessBoard& pos);

Score EvalDump(const ChessBoard& pos);

Score
EvaluateThreats(const ChessBoard& pos);


#endif

