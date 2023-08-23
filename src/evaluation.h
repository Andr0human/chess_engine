
#ifndef EVALUATION_H
#define EVALUATION_H


#include "movegen.h"
#include "PieceSquareTable.h"


class EvalData
{
    public:

    int w_pawns, w_knights, w_bishops, w_rooks, w_queens, w_pieces;
    int b_pawns, b_knights, b_bishops, b_rooks, b_queens, b_pieces;

    int pieceCount;
    int boardWeight;
    int gamePhase;

    Score pawnStructureScore;

    constexpr static float materialWeight = 1.0f;
    constexpr static float pieceTableWeight = 1.8f;
    constexpr static float threatsWeight = 0.5f;
    constexpr static float mobilityWeight = 1.2f;
    constexpr static float pawnSructureWeight = 0.4f;

    EvalData(const ChessBoard& pos)
    {
        w_pawns   = popcount(pos.piece(WHITE, PAWN  ));
        w_bishops = popcount(pos.piece(WHITE, BISHOP));
        w_knights = popcount(pos.piece(WHITE, KNIGHT));
        w_rooks   = popcount(pos.piece(WHITE, ROOK  ));
        w_queens  = popcount(pos.piece(WHITE, QUEEN ));

        b_pawns   = popcount(pos.piece(BLACK, PAWN  ));
        b_bishops = popcount(pos.piece(BLACK, BISHOP));
        b_knights = popcount(pos.piece(BLACK, KNIGHT));
        b_rooks   = popcount(pos.piece(BLACK, ROOK  ));
        b_queens  = popcount(pos.piece(BLACK, QUEEN ));

        w_pieces = w_bishops + w_knights + w_rooks + w_queens;
        b_pieces = b_bishops + b_knights + b_rooks + b_queens;
        pieceCount = w_pieces + b_pieces;

        gamePhase =
            10 * (w_pawns   + b_pawns)
          + 30 * (w_bishops + b_bishops + w_knights + b_knights)
          + 55 * (w_rooks   + b_rooks)
          + 90 * (w_queens  + b_queens);

        boardWeight =
            PawnValueMg   * (w_pawns   + b_pawns  )
          + BishopValueMg * (w_bishops + b_bishops)
          + KnightValueMg * (w_knights + b_knights)
          + RookValueMg   * (w_rooks   + b_rooks  )
          + QueenValueMg  * (w_queens  + b_queens );
    }
};


Score Evaluate(const ChessBoard& pos);


#endif

