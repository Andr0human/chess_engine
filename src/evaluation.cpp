
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
    int s_x = s & 7, s_y = s >> 3;
    int t_x = t & 7, t_y = t >> 3;
 
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
            score += strTable[NextSquare(piece)];
        return score;
    };

    Score pawns   = StrScore(pos.piece<WHITE, PAWN  >(), wpBoard)
                  - StrScore(pos.piece<BLACK, PAWN  >(), bpBoard);
    Score bishops = StrScore(pos.piece<WHITE, BISHOP>(), wBoard)
                  - StrScore(pos.piece<BLACK, BISHOP>(), bBoard);
    Score knights = StrScore(pos.piece<WHITE, KNIGHT>(), NBoard)
                  - StrScore(pos.piece<BLACK, KNIGHT>(), NBoard);
    Score rooks   = StrScore(pos.piece<WHITE, ROOK  >(), wRBoard)
                  - StrScore(pos.piece<BLACK, ROOK  >(), bRBoard);
    Score king    = StrScore(pos.piece<WHITE, KING  >(), WhiteKingMidGameTable)
                  - StrScore(pos.piece<BLACK, KING  >(), BlackKingMidGameTable);

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
            squares |= __f(NextSquare(piece), occupied);
        return PopCount(squares);
    };

    Score bishops = CalculateMobility(BishopAttackSquares, pos.piece<WHITE, BISHOP>())
                  - CalculateMobility(BishopAttackSquares, pos.piece<BLACK, BISHOP>());

    Score knights = CalculateMobility(KnightAttackSquares, pos.piece<WHITE, KNIGHT>())
                  - CalculateMobility(KnightAttackSquares, pos.piece<BLACK, KNIGHT>());

    Score rooks = CalculateMobility(RookAttackSquares, pos.piece<WHITE, ROOK>())
                - CalculateMobility(RookAttackSquares, pos.piece<BLACK, ROOK>());

    Score queens = CalculateMobility(QueenAttackSquares, pos.piece<WHITE, QUEEN>())
                 - CalculateMobility(QueenAttackSquares, pos.piece<BLACK, QUEEN>());

    return bishops + (2 * knights) + rooks + queens;
}

template <Color c_my>
static Score
PawnStructure(const ChessBoard& pos)
{
    constexpr Color c_emy = ~c_my;
    Score score = 0;
    Bitboard     pawns = pos.piece<c_my , PAWN>();
    Bitboard emy_pawns = pos.piece<c_emy, PAWN>();
    Bitboard    column = FileA;

    // Punish Double Pawns on same column
    for (int i = 0; i < 7; i++)
    {
        int p = PopCount(column & pawns);
        score -= 36 * p * (p - 1);
        column <<= 1;
    }


    while (pawns != 0)
    {
        Square pawn_sq = NextSquare(pawns);
        if ((plt::PassedPawnMasks[c_my][pawn_sq] & emy_pawns) != 0)
            continue;

        Score d = (7 * (c_my ^ 1)) + (pawn_sq >> 3) * (2 * c_my - 1);

        // Reward for having passed pawn
        score += 3 * d * d;

        Square  kpos = SquareNo(pos.piece<c_my , KING>());
        Square ekpos = SquareNo(pos.piece<c_emy, KING>());

        // Add score for king close to passed pawn and
        // Reduce score if enemy king is close to pawn
        int dist = (14 - Dist(pawn_sq, kpos)) - (14 - Dist(pawn_sq, ekpos));
        score += 6 * dist;
    }

    return score;
}

template<Color side>
static Score
AttackStrength(const ChessBoard& pos)
{
    Bitboard occupied = pos.All();
    // Square of enemy king
    Square ek_pos = SquareNo( pos.piece< ~side, KING>() );
    Bitboard king_mask = plt::KingMasks[ek_pos];

    const auto AddAttackers = [&] (const auto& __f, Bitboard piece, Score value)
    {
        Score score = 0;
        while (piece > 0) {
            Square ip = NextSquare(piece);
            Bitboard squares = __f(ip, occupied);

            if ((squares & king_mask) != 0)
                score += value;
        }
        return score;
    };

    int attackers = 0;

    attackers += AddAttackers(BishopAttackSquares, pos.piece<side, BISHOP>(), 1);
    attackers += AddAttackers(KnightAttackSquares, pos.piece<side, KNIGHT>(), 1);
    attackers += AddAttackers(RookAttackSquares  , pos.piece<side, ROOK  >(), 1);
    attackers += AddAttackers(QueenAttackSquares , pos.piece<side, QUEEN >(), 2);

    return 6 * attackers * (attackers + 1);
}

template<Color side>
static Score
KingSafety(const ChessBoard& pos)
{
    Square k_pos = SquareNo( pos.piece<side, KING>() );
    int kx = k_pos & 7;

    Bitboard* mask = (side == WHITE ? plt::UpMasks : plt::DownMasks);
    Bitboard pawns = pos.piece<side, PAWN>();

    int open_files = 0;
    int defenders = 0;

    if ((mask[k_pos] & pawns) == 0)
        open_files++;
    if ((kx != 0) and ((mask[k_pos + 1] & pawns) == 0))
        open_files++;
    if ((kx != 7) and ((mask[k_pos - 1] & pawns) == 0))
        open_files++;

    defenders += PopCount(pawns & mask[k_pos]);
    defenders += 2 * PopCount( mask[k_pos] & (
        pos.piece<side, BISHOP>() | pos.piece<side, KNIGHT>() | pos.piece<side, ROOK>()
    ));

    Score score = (9 * defenders * (defenders + 1)) - (30 * open_files * open_files);
    return score;
}

static Score
Threats(const ChessBoard& pos)
{
    // int attackWhite = AttackStrength<WHITE>(pos) * KingSafety(pos, BLACK);
    // int attackBlack = AttackStrength<BLACK>(pos) * KingSafety(pos, WHITE);


    return (AttackStrength<WHITE>(pos) + KingSafety<WHITE>(pos))
         - (AttackStrength<BLACK>(pos) + KingSafety<BLACK>(pos));
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
MaterialDiffereceEndGame(const EvalData& ed)
{
    return PawnValueEg   * (ed.w_pawns   - ed.b_pawns  )
         + BishopValueEg * (ed.w_bishops - ed.b_bishops)
         + KnightValueEg * (ed.w_knights - ed.b_knights)
         + RookValueEg   * (ed.w_rooks   - ed.b_rooks  )
         + QueenValueEg  * (ed.w_queens  - ed.b_queens );
}

static Score
DistanceBetweenKingsScore(const ChessBoard& pos, const EvalData& ed)
{
    Square wk_sq = SquareNo( pos.piece<WHITE, KING>() );
    Square bk_sq = SquareNo( pos.piece<BLACK, KING>() );

    int material_diff =
        + 3 * (ed.w_bishops - ed.b_bishops) + 3 * (ed.w_knights - ed.b_knights)
        + 5 * (ed.w_rooks   - ed.b_rooks  ) + 9 * (ed.w_queens  - ed.b_queens );

    int distance = 14 - Dist(wk_sq, bk_sq);
    Score score = (distance / 4) * (distance + 2) * material_diff;
    return score;
}

template <Color winningSide>
static Score
BishopColorCornerScore(const ChessBoard& pos)
{
    Bitboard bishops   = pos.piece<  winningSide, BISHOP>();
    Bitboard won_king  = pos.piece<  winningSide, KING>();
    Bitboard lost_king = pos.piece< ~winningSide, KING>();

    if (PopCount(bishops) != 1)
        return VALUE_ZERO;

    Score score = VALUE_ZERO;

    Bitboard endSquares =
        ((bishops & WhiteSquares) != 0 ? WhiteColorCorner : NoSquares)
      | ((bishops & BlackSquares) != 0 ? BlackColorCorner : NoSquares);


    Bitboard whiteEnd = (1ULL << SQ_A8) | (1ULL << SQ_H1);
    Bitboard blackEnd = (1ULL << SQ_A1) | (1ULL << SQ_H8);

    Bitboard whiteDangerSquare = (1ULL << SQ_C6) | (1ULL << SQ_F3);
    Bitboard blackDangerSquare = (1ULL << SQ_C3) | (1ULL << SQ_F6);

    Score correctSideForCheckmateScore = 520;

    if ((endSquares & lost_king) != 0)
        score += correctSideForCheckmateScore * (2 * winningSide - 1);

    if ((((lost_king & whiteEnd) != 0) and ((won_king & whiteDangerSquare) != 0))
     or (((lost_king & blackEnd) != 0) and ((won_king & blackDangerSquare) != 0))) score -= 372 * (2 * winningSide - 1);

    return score;
}

template <Color winningSide>
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

    // Color winningSide = (ed.w_pawns + ed.w_pieces > 0) ? WHITE : BLACK;
    constexpr Color losingSide  = ~winningSide;

    Square  wonKingSq = SquareNo( pos.piece<winningSide, KING>() );
    Square lostKingSq = SquareNo( pos.piece<losingSide , KING>() );

    Score winningSideCorrectionFactor = 2 * winningSide - 1;
    Score  losingSideCorrectionFactor = 2 *  losingSide - 1;

    Score distanceScore = DistanceBetweenKingsScore(pos, ed);
    Score parityScore   = BishopColorCornerScore<winningSide>(pos);
    Score centreScore   = LongKingWinningEndGameTable[wonKingSq] * winningSideCorrectionFactor
                        + LoneKingLosingEndGameTable[lostKingSq] * losingSideCorrectionFactor;
    Score materialScore = MaterialDiffereceEndGame(ed);

    Score score = materialScore + distanceScore + parityScore + centreScore;
    int side2move = 2 * int(pos.color) - 1;

    return score * side2move;
}

static Score
PieceTableStrengthEndGame(const ChessBoard& pos)
{
    const auto StrScore = [] (Bitboard piece, int* strTable)
    {
        Score score = 0;
        while (piece > 0)
            score += strTable[NextSquare(piece)];
        return score;
    };

    Score king = StrScore(pos.piece<WHITE, KING>(), KingEndGameTable)
               - StrScore(pos.piece<BLACK, KING>(), KingEndGameTable);
    return king;
}

static Score
ColorParityScore(const ChessBoard& pos)
{
    Bitboard w_king   = pos.piece<WHITE, KING  >();
    Bitboard b_king   = pos.piece<BLACK, KING  >();
    Bitboard w_bishop = pos.piece<WHITE, BISHOP>();
    Bitboard b_bishop = pos.piece<BLACK, BISHOP>();

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

    if ((ed.boardWeight == 0) or IsHypotheticalDraw(ed))
        return VALUE_DRAW;

    // Special EndGames
    if (ed.NoWhitePiecesOnBoard() or ed.NoBlackPiecesOnBoard())
        return (ed.w_pawns + ed.w_pieces > 0)
        ? LoneKingEndGame<WHITE>(pos, ed) : LoneKingEndGame<BLACK>(pos, ed);
    
    ed.pawnStructureScore = PawnStructure<WHITE>(pos) - PawnStructure<BLACK>(pos);

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


    if (ed.NoWhitePiecesOnBoard() or ed.NoBlackPiecesOnBoard()) {
        // cout << "In LoneKingEndgame!" << endl;
        // Color winningSide = (ed.w_pawns + ed.w_pieces > 0) ? WHITE : BLACK;
        // Color losingSide  = ~winningSide;

        // Square  wonKingSq = SquareNo( pos.piece(winningSide, KING) );
        // Square lostKingSq = SquareNo( pos.piece(losingSide , KING) );

        // Score winningSideCorrectionFactor = 2 * winningSide - 1;
        // Score  losingSideCorrectionFactor = 2 *  losingSide - 1;

        // Score distanceScore = DistanceBetweenKingsScore(pos, ed);
        // Score parityScore   = BishopColorCornerScore(pos, winningSide);
        // Score centreScore   = LongKingWinningEndGameTable[wonKingSq] * winningSideCorrectionFactor
        //                     + LoneKingLosingEndGameTable[lostKingSq] * losingSideCorrectionFactor;
        // Score materialScore = MaterialDiffereceEndGame(ed);

        // cout << "distanceScore = " << distanceScore << endl;
        // cout << "parityScore   = " << parityScore   << endl;
        // cout << "centreScore   = " << centreScore   << endl;
        // cout << "materialScore = " << materialScore << endl;

        // Score score = materialScore + distanceScore + parityScore + centreScore;
        // int side2move = 2 * int(pos.color) - 1;

        // cout << "finalScore = " << score * side2move << endl;
        // return score * side2move;
    }


    Score pawnStructrueWhite = PawnStructure<WHITE>(pos);
    Score pawnStructrueBlack = PawnStructure<WHITE>(pos);
    ed.pawnStructureScore = pawnStructrueWhite - pawnStructrueBlack;

    cout << "Score_pawn_structure_white = " << pawnStructrueWhite << endl;
    cout << "Score_pawn_structure_black = " << pawnStructrueBlack << endl;
    cout << "Score_pawn_structure_total = " << ed.pawnStructureScore << endl << endl;


    Score mg_score = 0, eg_score = 0;

    // MidGame
    Score materialScoreMid   = MaterialDiffereceMidGame(ed);
    Score pieceTableScoreMid = PieceTableStrengthMidGame(pos);
    Score mobilityScoreMid   = MobilityStrength(pos);

    Score attackWhite = AttackStrength<WHITE>(pos);
    Score attackBlack = AttackStrength<BLACK>(pos);
    Score safetyWhite = KingSafety<WHITE>(pos);
    Score safetyBlack = KingSafety<BLACK>(pos);

    Score threatsScoreMid = (attackWhite + safetyWhite) - (attackBlack + safetyBlack);

    cout << " -- MIDGAME -- " << endl;
    cout << "materialScore   = " << materialScoreMid   << endl;
    cout << "pieceTableScore = " << pieceTableScoreMid << endl;
    cout << "mobilityScore   = " << mobilityScoreMid   << endl << endl;

    cout << "threatsScore = " << threatsScoreMid
         << " | attackWhite  = " << attackWhite << " | safetyWhite  = " << safetyWhite
         << " | attackBlack  = " << attackBlack << " | safetyBlack  = " << safetyBlack
         << endl << endl;


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

