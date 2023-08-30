
#include "evaluation.h"

using std::abs;
using std::min, std::max;

// static int
// MinDist(Square s, Square t)
// {
//     Square s_x = s & 7, s_y = s >> 3;
//     Square t_x = t & 7, t_y = t >> 3;

//     return max(0, max(abs(s_x - t_x), abs(s_y - t_y)) - 1);
// }

static int
Dist(Square s, Square t)
{
    Square s_x = s & 7, s_y = s >> 3;
    Square t_x = t & 7, t_y = t >> 3;
 
    return abs(s_x - t_x) + abs(s_y - t_y);
}

static bool
IsHypotheticalDraw(const EvalData& ed)
{
    // No draws if pawns on the board
    if (ed.w_pawns + ed.b_pawns > 0)
        return false;

    if ((ed.w_pieces > 2) or (ed.b_pieces > 2))
        return false;

    // If one piece remains and its bishop or knight
    if (ed.w_pieces + ed.b_pieces == 1) {
        if ((ed.w_bishops + ed.b_bishops == 1)
         or (ed.w_knights + ed.b_knights == 1)) return true;
    }

    //! TODO Test for [WQ_BQ, WR_BR] positions

    // If one piece of both sides left, and its not [WQ_BR, WR_BQ]
    // Then it is a draw (almost)
    if ((ed.w_pieces == 1) and (ed.b_pieces == 1)) {
        if (((ed.w_queens > 0) and (ed.b_queens == 0))
         or ((ed.b_queens > 0) and (ed.w_queens == 0))) return false;

        return true;
    }

    if ((ed.w_pieces + ed.b_pieces == 2) and (ed.w_knights + ed.b_knights == 2))
        return true;

    return false;
}


#ifndef MIDGAME

static Score
MaterialDiffereceMidGame(const EvalData& ed)
{
    return PawnValueMg   * (ed.w_pawns   - ed.b_pawns  )
         + BishopValueMg * (ed.w_bishops - ed.b_bishops)
         + KnightValueMg * (ed.w_knights - ed.b_knights)
         + RookValueMg   * (ed.w_rooks   - ed.b_rooks  )
         + QueenValueMg  * (ed.w_queens  - ed.b_queens );
}

static Score
PieceTableStrengthMidGame(const ChessBoard& pos)
{
    const auto StrScore = [] (Bitboard piece, int* strTable)
    {
        Score score = 0;
        while (piece > 0)
            score += strTable[next_idx(piece)];
        return score;
    };

    Score pawns   = StrScore(pos.piece(WHITE, PAWN ), wpBoard)
                  - StrScore(pos.piece(BLACK, PAWN ), bpBoard);
    Score bishops = StrScore(pos.piece(WHITE, BISHOP), wBoard)
                  - StrScore(pos.piece(BLACK, BISHOP), bBoard);
    Score knights = StrScore(pos.piece(WHITE, KNIGHT), NBoard)
                  - StrScore(pos.piece(BLACK, KNIGHT), NBoard);
    Score rooks   = StrScore(pos.piece(WHITE, ROOK ), wRBoard)
                  - StrScore(pos.piece(BLACK, ROOK ), bRBoard);
    Score king    = StrScore(pos.piece(WHITE, KING), WhiteKingMidGameTable)
                  - StrScore(pos.piece(BLACK, KING), BlackKingMidGameTable);

    return pawns + bishops + knights + rooks + king;
}

static Score
MobilityStrength(const ChessBoard& pos)
{
    const auto CalculateMobility = [&] (const auto& __f, Bitboard piece)
    {
        Bitboard occupied = pos.All();
        Bitboard squares = 0;
        while (piece > 0)
            squares |= __f(next_idx(piece), occupied);
        return popcount(squares);
    };

    Score bishops = CalculateMobility(bishop_atk_sq, pos.piece(WHITE, BISHOP))
                  - CalculateMobility(bishop_atk_sq, pos.piece(BLACK, BISHOP));

    Score knights = CalculateMobility(knight_atk_sq, pos.piece(WHITE, KNIGHT))
                  - CalculateMobility(knight_atk_sq, pos.piece(BLACK, KNIGHT));

    Score rooks = CalculateMobility(rook_atk_sq, pos.piece(WHITE, ROOK))
                - CalculateMobility(rook_atk_sq, pos.piece(BLACK, ROOK));

    Score queens = CalculateMobility(queen_atk_sq, pos.piece(WHITE, QUEEN))
                 - CalculateMobility(queen_atk_sq, pos.piece(BLACK, QUEEN));

    return bishops + (2 * knights) + rooks + queens;
}

static Score
PawnStructure(const ChessBoard& pos, Color color)
{
    Score score = 0;
    Bitboard     pawns = pos.piece( color, PAWN);
    Bitboard emy_pawns = pos.piece(~color, PAWN);
    Bitboard    column = FileA;

    // Punish Double Pawns on same column
    for (int i = 0; i < 7; i++)
    {
        int p = popcount(column & pawns);
        score -= 36 * p * (p - 1);
        column <<= 1;
    }


    while (pawns != 0)
    {
        Square pawn_sq = next_idx(pawns);
        if ((plt::PassedPawnMasks[color][pawn_sq] & emy_pawns) != 0)
            continue;

        Score d = (7 * (color ^ 1)) + (pawn_sq >> 3) * (2 * color - 1);

        // Reward for having passed pawn
        score += 3 * d * d;

        Square  kpos = idx_no(pos.piece( color, KING));
        Square ekpos = idx_no(pos.piece(~color, KING));

        // Add score for king close to passed pawn and
        // Reduce score if enemy king is close to pawn
        int dist = (14 - Dist(pawn_sq, kpos)) - (14 - Dist(pawn_sq, ekpos));
        score += 6 * dist;
    }

    return score;
}

static Score
AttackStrength(const ChessBoard& pos, Color side)
{
    Bitboard occupied = pos.All();
    // Square of enemy king
    Square ek_pos = idx_no( pos.piece(~side, KING) );
    Bitboard king_mask = plt::KingMasks[ek_pos];

    const auto AddAttackers = [&] (const auto& __f, Bitboard piece, Score value)
    {
        Score score = 0;
        while (piece > 0) {
            Square ip = next_idx(piece);
            Bitboard squares = __f(ip, occupied);

            if ((squares & king_mask) != 0)
                score += value;
        }
        return score;
    };

    int attackers = 0;

    attackers += AddAttackers(bishop_atk_sq, pos.piece(side, BISHOP), 1);
    attackers += AddAttackers(knight_atk_sq, pos.piece(side, KNIGHT), 1);
    attackers += AddAttackers(rook_atk_sq  , pos.piece(side, ROOK  ), 1);
    attackers += AddAttackers(queen_atk_sq , pos.piece(side, QUEEN ), 2);

    return 6 * attackers * (attackers + 1);
}

static Score
KingSafety(const ChessBoard& pos, Color side)
{
    Square k_pos = idx_no( pos.piece(side, KING) );
    Square kx = k_pos & 7;

    Bitboard* mask = (side == WHITE ? plt::UpMasks : plt::DownMasks);
    Bitboard pawns = pos.piece(side, PAWN);

    int open_files = 0;
    int defenders = 0;

    if ((mask[k_pos] & pawns) == 0)
        open_files++;
    if ((kx != 0) and ((mask[k_pos + 1] & pawns) == 0))
        open_files++;
    if ((kx != 7) and ((mask[k_pos - 1] & pawns) == 0))
        open_files++;

    defenders += popcount(pawns & mask[k_pos]);
    defenders += 2 * popcount( mask[k_pos] & (
        pos.piece(side, BISHOP) | pos.piece(side, KNIGHT) | pos.piece(side, ROOK)
    ));

    Score score = (9 * defenders * (defenders + 1)) - (30 * open_files * open_files);
    return score;
}

static Score
Threats(const ChessBoard& pos)
{
    return (AttackStrength(pos, WHITE) + KingSafety(pos, WHITE))
         - (AttackStrength(pos, BLACK) + KingSafety(pos, BLACK));
}

static Score
MidGameScore(const ChessBoard& pos, const EvalData& ed)
{
    Score materialScore   = MaterialDiffereceMidGame(ed);
    Score pieceTableScore = PieceTableStrengthMidGame(pos);
    Score mobilityScore   = MobilityStrength(pos);
    Score pawnStructure   = ed.pawnStructureScore;
    Score threatsScore    = Threats(pos);

    float eval =
        ed.materialWeight     * float(materialScore)
      + ed.pieceTableWeight   * float(pieceTableScore)
      + ed.mobilityWeight     * float(mobilityScore)
      + ed.pawnSructureWeight * float(pawnStructure)
      + ed.threatsWeight      * float(threatsScore);

    return Score(eval);
}


#endif

#ifndef ENDGAME

static Score
DistanceBetweenKingsScore(const ChessBoard& pos, const EvalData& ed)
{
    Square wk_sq = idx_no( pos.piece(WHITE, KING) );
    Square bk_sq = idx_no( pos.piece(BLACK, KING) );

    int material_diff =
        + 3 * (ed.w_bishops - ed.b_bishops) + 3 * (ed.w_knights - ed.b_knights)
        + 5 * (ed.w_rooks   - ed.b_rooks  ) + 9 * (ed.w_queens  - ed.b_queens );

    int distance = 14 - Dist(wk_sq, bk_sq);
    Score score = (distance / 2) * (distance + 2) * material_diff;
    return score;
}

static Score
BishopColorCornerScore(const ChessBoard& pos, Color winningSide)
{
    Bitboard bishops  = pos.piece(winningSide, BISHOP);
    Bitboard emy_king = pos.piece(~winningSide, KING);

    Bitboard endSquares =
        ((bishops & WhiteSquares) != 0 ? WhiteColorCorner : NoSquares)
      | ((bishops & BlackSquares) != 0 ? BlackColorCorner : NoSquares);

    Score correctSideForCheckmateScore = 120;

    return ((endSquares & emy_king) != 0)
        ? (correctSideForCheckmateScore * winningSide) : VALUE_ZERO;
}

static Score
LoneKingEndGame(const ChessBoard& pos, const EvalData& ed)
{
    /**
     * Evaluates the score for an endgame position where the 
     * losing side has no major, minor pieces, or pawns left.
     * 
     * Intuition:
     * In this endgame scenario, the primary objective is to 
     * corner the king of the losing side. Simultaneously, 
     * bringing the winning side's king closer to the opponent's
     * king for checkmate setup.
     * 
     * If the winning side has a bishop, the strategy for checkmate  
     * corner differs based on the bishop's color. If white bishop 
     * bring the losing king to white corners. Conversely,
     * if the bishop is black, focus on bringing the losing side
     * king to black corner.
    **/
    // Note : Failed so far

    Color winningSide = (ed.w_pawns + ed.w_pieces > 0) ? WHITE : BLACK;
    Color losingSide  = ~winningSide;

    Square loneKingSq = idx_no( pos.piece(losingSide, KING) );

    Score distanceScore = DistanceBetweenKingsScore(pos, ed);
    Score parityScore   = BishopColorCornerScore(pos, winningSide);
    Score centreScore   = losingSide * LoneKingEndGameTable[loneKingSq];

    return distanceScore + parityScore + centreScore;
}

static Score
MaterialDiffereceEndGame(const EvalData& ed)
{
    return PawnValueEg   * (ed.w_pawns   - ed.b_pawns  )
         + BishopValueEg * (ed.w_bishops - ed.b_bishops)
         + KnightValueEg * (ed.w_knights - ed.b_knights)
         + RookValueEg   * (ed.w_rooks   - ed.b_rooks  )
         + QueenValueEg  * (ed.w_queens  - ed.b_queens );
}

static Score
PieceTableStrengthEndGame(const ChessBoard& pos)
{
    const auto StrScore = [] (Bitboard piece, int* strTable)
    {
        Score score = 0;
        while (piece > 0)
            score += strTable[next_idx(piece)];
        return score;
    };

    Score king = StrScore(pos.piece(WHITE, KING), KingEndGameTable)
               - StrScore(pos.piece(BLACK, KING), KingEndGameTable);
    return king;
}

static Score
ColorParityScore(const ChessBoard& pos)
{
    Bitboard w_king   = pos.piece(WHITE, KING  );
    Bitboard b_king   = pos.piece(BLACK, KING  );
    Bitboard w_bishop = pos.piece(WHITE, BISHOP);
    Bitboard b_bishop = pos.piece(BLACK, BISHOP);

    Bitboard endSquaresForWhite =
        ((w_bishop & WhiteSquares) != 0 ? WhiteColorCorner : NoSquares)
      | ((w_bishop & BlackSquares) != 0 ? BlackColorCorner : NoSquares);

    Bitboard endSquaresForBlack =
        ((b_bishop & WhiteSquares) != 0 ? WhiteColorCorner : NoSquares)
      | ((b_bishop & BlackSquares) != 0 ? BlackColorCorner : NoSquares);

    Score score = 0;
    Score incForCorrectSide = 40;

    if ((b_king & endSquaresForWhite) != 0)
        score += incForCorrectSide;

    if ((w_king & endSquaresForBlack) != 0)
        score -= incForCorrectSide;

    return score;
}

static Score
EndGameScore(const ChessBoard& pos, const EvalData& ed)
{
    // Distance between kings
    // King in corners
    // BN endgames
    // Rule of Square (2n1k1r1/p7/3B1Rp1/2P2pKp/8/4P1P1/5P1P/8 w - - 17 45)

    Score materialScore   = MaterialDiffereceEndGame(ed);
    Score pieceTableScore = PieceTableStrengthEndGame(pos);
    Score pawnStructure   = ed.pawnStructureScore;
    Score distanceScore   = DistanceBetweenKingsScore(pos, ed);
    Score parityScore     = ColorParityScore(pos);

    float eval =
        ed.materialWeight     * float(materialScore)
      + ed.pieceTableWeight   * float(pieceTableScore)
      + ed.pawnSructureWeight * float(pawnStructure)
      + float(distanceScore)  + float(parityScore);

    return Score(eval);
}

#endif


Score
Evaluate(const ChessBoard& pos)
{
    EvalData ed = EvalData(pos);
    int side2move = 2 * int(pos.color) - 1;

    // Special EndGames
    // if (ed.NoWhitePiecesOnBoard() or ed.NoBlackPiecesOnBoard())
    //     return LoneKingEndGame(pos, ed);

    if ((ed.boardWeight == 0) or IsHypotheticalDraw(ed))
        return VALUE_DRAW;
    
    ed.pawnStructureScore = PawnStructure(pos, WHITE) - PawnStructure(pos, BLACK);

    Score mg_score = MidGameScore(pos, ed);
    Score eg_score = EndGameScore(pos, ed);

    float phase = ed.phase;
    Score score = Score( phase * float(mg_score) + (1 - phase) * float(eg_score) );
    return score * side2move;
}


Score EvalDump(const ChessBoard& pos)
{
    EvalData ed = EvalData(pos);
    float phase = ed.phase;

    cout << "----------------------------------------------" << endl;
    cout << "BoardWeight = " << ed.boardWeight << endl;
    cout << "Phase = " << phase << endl;

    if (IsHypotheticalDraw(ed))
    {
        cout << "Position in Hypothetical Draw!" << endl;
        return VALUE_DRAW;
    }

    Score pawnStructrueWhite = PawnStructure(pos, WHITE);
    Score pawnStructrueBlack = PawnStructure(pos, BLACK);
    ed.pawnStructureScore = pawnStructrueWhite - pawnStructrueBlack;

    cout << "Score_pawn_structure_white = " << pawnStructrueWhite << endl;
    cout << "Score_pawn_structure_black = " << pawnStructrueBlack << endl;
    cout << "Score_pawn_structure_total = " << ed.pawnStructureScore << endl << endl;


    Score mg_score = 0, eg_score = 0;

    // MidGame
    Score materialScoreMid   = MaterialDiffereceMidGame(ed);
    Score pieceTableScoreMid = PieceTableStrengthMidGame(pos);
    Score mobilityScoreMid   = MobilityStrength(pos);
    Score threatsScoreMid    = Threats(pos);

    cout << " -- MIDGAME -- " << endl;
    cout << "materialScore   = " << materialScoreMid   << endl;
    cout << "pieceTableScore = " << pieceTableScoreMid << endl;
    cout << "mobilityScore   = " << mobilityScoreMid   << endl;
    cout << "threatsScore    = " << threatsScoreMid    << endl << endl;


    mg_score = Score(
        ed.materialWeight     * float(materialScoreMid)
      + ed.pieceTableWeight   * float(pieceTableScoreMid)
      + ed.mobilityWeight     * float(mobilityScoreMid)
      + ed.pawnSructureWeight * float(ed.pawnStructureScore)
      + ed.threatsWeight      * float(threatsScoreMid) );

    // Endgame
    Score materialScore   = MaterialDiffereceEndGame(ed);
    Score pieceTableScore = PieceTableStrengthEndGame(pos);
    Score distanceScore   = DistanceBetweenKingsScore(pos, ed);
    Score parityScore     = ColorParityScore(pos);

    eg_score = Score(
        ed.materialWeight     * float(materialScore)
      + ed.pieceTableWeight   * float(pieceTableScore)
      + ed.pawnSructureWeight * float(ed.pawnStructureScore)
      + float(distanceScore)  + float(parityScore) );

    cout << " -- ENDGAME -- " << endl;
    cout << "materialScore   = " << materialScore   << endl;
    cout << "pieceTableScore = " << pieceTableScore << endl;
    cout << "distanceScore   = " << distanceScore   << endl;
    cout << "parityScore     = " << parityScore     << endl;

    
    Score score = Score( phase * float(mg_score) + (1 - phase) * float(eg_score) );

    cout << "mg_score = " << mg_score << endl;
    cout << "eg_score = " << eg_score << endl;
    cout << "score    = " << score << endl;
    cout << "----------------------------------------------" << endl;

    return score;
}

