
#include "evaluation.h"
#include "attacks.h"

using std::abs;
using std::min;

template <Color side>
static bool
OnePawnEndgame(const ChessBoard& pos)
{
  // Assuming its white
  constexpr Color emySide = ~side;

  const Bitboard    pawn = pos.piece<side   , PAWN>();
  const Bitboard  myKing = pos.piece<side   , KING>();
  const Bitboard emyKing = pos.piece<emySide, KING>();

  const int incFactor = 2 * side - 1;

  const Square pawnSq    = SquareNo(pawn   );
  const Square myKingSq  = SquareNo(myKing );
  const Square emyKingSq = SquareNo(emyKing);

//   8/4k3/8/4K3/4P3/8/8/8 w - - 0 1

  if ((pawnSq + 8 * incFactor == emyKingSq) and
      (pawnSq - 8 * incFactor ==  myKingSq) and
      ((pos.color == emySide) or (!(emyKing & Rank18) and (pos.color == side)))
    ) return true;

  if ((pawnSq + 8 * incFactor == emyKingSq) and
      ((pawnSq - 7 * incFactor == myKingSq) or (pawnSq - 9 * incFactor == myKingSq)) and
      ((pos.color == side) or (!(emyKing & Rank18) and (pos.color == emySide)))
    ) return true;

  if ((pawnSq +  8 * incFactor ==  myKingSq) and
      (pawnSq + 24 * incFactor == emyKingSq) and
      ((pos.color == side) and !(emyKing & Rank18))
    ) return true;

  return false;
}

static int
Distance(Square s, Square t)
{
    int s_x = s & 7, s_y = s >> 3;
    int t_x = t & 7, t_y = t >> 3;

    return abs(s_x - t_x) + abs(s_y - t_y);
}

bool
isTheoreticalDraw(const ChessBoard& pos)
{
    int pieceCount = pos.pieceCount<WHITE, ALL>() + pos.pieceCount<BLACK, ALL>() - 2;
    if (pieceCount > 2)
        return false;

    // KPK position
    if (pieceCount == 0)
        return true;

    if (pieceCount == 1)
    {
        if (pos.pieceCount<WHITE, PAWN>() + pos.piece<BLACK, PAWN>() > 0)
            return pos.pieceCount<WHITE, PAWN>() == 1 ? OnePawnEndgame<WHITE>(pos) : OnePawnEndgame<BLACK>(pos);

        if ((pos.pieceCount<WHITE, BISHOP>() + pos.pieceCount<BLACK, BISHOP>() == 1)
         or (pos.pieceCount<WHITE, KNIGHT>() + pos.pieceCount<BLACK, KNIGHT>() == 1)) return true;

        return false;
    }

    if (pos.pieceCount<WHITE, ALL>() == 2 and pos.pieceCount<BLACK, ALL>() == 2)
    {
        if (((pos.pieceCount<WHITE, QUEEN>() > 0) and (pos.pieceCount<BLACK, QUEEN>() == 0))
         or ((pos.pieceCount<BLACK, QUEEN>() > 0) and (pos.pieceCount<WHITE, QUEEN>() == 0))) return false;
        return true;
    }

    if (pos.pieceCount<WHITE, KNIGHT>() + pos.pieceCount<BLACK, KNIGHT>() == 2) return true;
    return false;
}

template <Color c_my>
static bool
CannotWin(const ChessBoard& pos, const EvalData& ed)
{
    if (pos.pieceCount<c_my, PAWN>() > 0)
        return false;

    if (ed.pieces[c_my] == 1)
        return pos.pieceCount<c_my, KNIGHT>() + pos.pieceCount<c_my, BISHOP>() == 1;

    if ((ed.pieces[c_my] == 2) and (pos.pieceCount<c_my, KNIGHT>() == 2))
        return true;
}


#ifndef THREATS

template <Color c_my, PieceType pt, Bitboard* mask, Score increment>
static Score
AttacksKing(const ChessBoard& pos)
{
    Score score = VALUE_ZERO;
    Bitboard piece_bb = pos.piece<c_my, pt>();
    Bitboard occupied = pos.All();
    Square king_sq_emy = SquareNo(pos.piece< ~c_my, KING>());

    while (piece_bb != 0)
    {
        Square sq = NextSquare(piece_bb);
        if ((AttackSquares<pt>(sq, occupied) & mask[king_sq_emy]) != 0)
            score += increment;
    }

    return score;
}

template <Color c_my>
static Score
AttackValue(const ChessBoard& pos)
{
    using plt::KingMasks;
    using plt::KingOuterMasks;

    Score attack_value = VALUE_ZERO;

    attack_value += AttacksKing<c_my, KNIGHT, KingMasks, 2>(pos);
    attack_value += AttacksKing<c_my, BISHOP, KingMasks, 3>(pos);
    attack_value += AttacksKing<c_my, ROOK  , KingMasks, 4>(pos);
    attack_value += AttacksKing<c_my, QUEEN , KingMasks, 6>(pos);

    attack_value += AttacksKing<c_my, KNIGHT, KingOuterMasks, 1>(pos);
    attack_value += AttacksKing<c_my, BISHOP, KingOuterMasks, 2>(pos);
    attack_value += AttacksKing<c_my, ROOK  , KingOuterMasks, 3>(pos);
    attack_value += AttacksKing<c_my, QUEEN , KingOuterMasks, 4>(pos);

    return attack_value / 4;
}


template <Color c_my, PieceType pt, int pieceVal>
static Score
CalcDistanceScore(const ChessBoard& pos)
{
    Bitboard piece_bb = pos.piece<c_my, pt>();
    Square emyKingSq = SquareNo(pos.piece<~c_my, KING>());

    Score score = VALUE_ZERO;

    while (piece_bb != 0) {
        score += (pieceVal * (1 << (14 - Distance(NextSquare(piece_bb), emyKingSq)))) >> 7;
        // int d = 14 - Distance(NextSquare(piece_bb), emyKingSq);
        // score = (d * d) / 2;
    }

    return score;
}

template <Color c_my>
static Score
AttackDistanceScore(const ChessBoard& pos)
{
    Score distance_score = VALUE_ZERO;

    distance_score += CalcDistanceScore<c_my, PAWN  , 1>(pos);
    distance_score += CalcDistanceScore<c_my, KNIGHT, 2>(pos);
    distance_score += CalcDistanceScore<c_my, BISHOP, 3>(pos);
    distance_score += CalcDistanceScore<c_my, ROOK  , 4>(pos);
    distance_score += CalcDistanceScore<c_my, QUEEN , 7>(pos);

    return distance_score;
}

template <Color c_my>
static Score
OpenFilesScore(const ChessBoard& pos)
{
    Score score = VALUE_ZERO;
    Bitboard column_bb = FileA;

    int king_col = SquareNo(pos.piece<c_my, KING>()) & 7;
    Bitboard pawns = pos.piece<WHITE, PAWN>() | pos.piece<BLACK, PAWN>();

    for (int col = 0; col < 8; col++)
    {
        if ((column_bb & pawns) == 0) {
            score += (1 << (7 - abs(col - king_col))) / 2;
        }
        column_bb <<= 1;
    }
    return (score / 4) + 1;
}

template <Color c_my, PieceType pt>
static Bitboard
GenAttackedSquares(const ChessBoard& pos)
{
    Bitboard squares = 0;
    Bitboard piece_bb = pos.piece<c_my, pt>();
    Bitboard occupied = pos.All();

    while (piece_bb != 0)
        squares |= AttackSquares<pt>(NextSquare(piece_bb), occupied);
    return squares;
}

template <Color c_my>
static Score
KingMobilityScore(const ChessBoard& pos)
{
    Square k_sq = SquareNo(pos.piece<c_my, KING>());
    Bitboard pieces_my = pos.piece<c_my, ALL>();
    Bitboard attacked_squares = 0;

    attacked_squares |= PawnAttackSquares<~c_my>(pos);
    attacked_squares |= GenAttackedSquares<~c_my, BISHOP>(pos);
    attacked_squares |= GenAttackedSquares<~c_my, KNIGHT>(pos);
    attacked_squares |= GenAttackedSquares<~c_my, ROOK>(pos);
    attacked_squares |= GenAttackedSquares<~c_my, QUEEN>(pos);
    attacked_squares |= GenAttackedSquares<~c_my, KING>(pos);

    int __x = PopCount(AttackSquares<KING>(k_sq, 0) & ~(pieces_my | attacked_squares));
    return min(__x, 3);
}

template <Color c_my>
static Score
AttackersLeft(const ChessBoard& pos)
{
    return Score(
        pos.pieceCount<c_my, KNIGHT>() + 2 * pos.pieceCount<c_my, BISHOP>()
      + 3 * pos.pieceCount<c_my, ROOK>() + 5 * pos.pieceCount<c_my, QUEEN>() );
}


template <Color c_my>
static Score
DefendersCount(const ChessBoard& pos)
{
    Square k_sq = SquareNo(pos.piece<c_my, KING>());
    //Pawns in front of king
    Bitboard pawns = pos.piece<c_my, PAWN>();
    Bitboard mask = plt::PawnMasks[c_my][k_sq] | plt::PawnCaptureMasks[c_my][k_sq];

    Bitboard pieces = (pos.piece<c_my, BISHOP>() | pos.piece<c_my, KNIGHT>() | pos.piece<c_my, ROOK>())
        & (plt::KingMasks[k_sq] | plt::KingOuterMasks[k_sq]);

    return 2 * PopCount(mask & pawns) + PopCount(pieces);
}

static Score
Threats(const ChessBoard& pos)
{
    // Attack Value Currently
    //    - Distance of pieces from king
    //    - Whether pieces attack the king
    // King Safety
    //    - King Mobility
    //    - Defenders
    //    - Open files
    // Long-term prospect
    //    - No. of attackers left
    //    - Open files

    // Increase Attack Value if lack of KIngSafety
    // Threat = Attack_Value * Lack_Of_Safety + Long_Term_Prospects

    Score attackValueWhite = AttackValue<WHITE>(pos);
    Score attackValueBlack = AttackValue<BLACK>(pos);

    Score distanceScoreWhite = AttackDistanceScore<WHITE>(pos);
    Score distanceScoreBlack = AttackDistanceScore<BLACK>(pos);

    Score kingMobilityWhite = KingMobilityScore<WHITE>(pos);
    Score kingMobilityBlack = KingMobilityScore<BLACK>(pos);

    Score openFileDeductionWhite = OpenFilesScore<WHITE>(pos);
    Score openFileDeductionBlack = OpenFilesScore<BLACK>(pos);

    Score attackersLeftWhite = AttackersLeft<WHITE>(pos);
    Score attackersLeftBlack = AttackersLeft<BLACK>(pos);

    Score defendersCountWhite = DefendersCount<WHITE>(pos);
    Score defendersCountBlack = DefendersCount<BLACK>(pos);

    Score lackOfSafetyWhite = (openFileDeductionWhite * (4 - kingMobilityWhite)) / (defendersCountWhite + 1);
    Score lackOfSafetyBlack = (openFileDeductionBlack * (4 - kingMobilityBlack)) / (defendersCountBlack + 1);

    Score currentAttackWhite = attackValueWhite * lackOfSafetyBlack + (distanceScoreWhite / (defendersCountBlack + 1));
    Score currentAttackBlack = attackValueBlack * lackOfSafetyWhite + (distanceScoreBlack / (defendersCountBlack + 1));

    Score longTermAttackWhite = ((attackersLeftWhite * attackersLeftWhite) + (openFileDeductionBlack * openFileDeductionBlack)) / (32 + defendersCountBlack);
    Score longTermAttackBlack = ((attackersLeftBlack * attackersLeftBlack) + (openFileDeductionWhite * openFileDeductionWhite)) / (32 + defendersCountWhite);

    return (currentAttackWhite + longTermAttackWhite) - (currentAttackBlack + longTermAttackBlack);
}


#endif

#ifndef MIDGAME

static Score
MaterialDiffereceMidGame(const ChessBoard& pos)
{
    return PawnValueMg   * (pos.pieceCount<WHITE, PAWN  >() - pos.pieceCount<BLACK, PAWN  >())
		 + BishopValueMg * (pos.pieceCount<WHITE, BISHOP>() - pos.pieceCount<BLACK, BISHOP>())
		 + KnightValueMg * (pos.pieceCount<WHITE, KNIGHT>() - pos.pieceCount<BLACK, KNIGHT>())
		 + RookValueMg   * (pos.pieceCount<WHITE, ROOK  >() - pos.pieceCount<BLACK, ROOK  >())
		 + QueenValueMg  * (pos.pieceCount<WHITE, QUEEN >() - pos.pieceCount<BLACK, QUEEN >());
}

template<Color c_my, PieceType pt, Score* strTable>
static Score
AddStrScore(const ChessBoard& pos)
{
    Bitboard piece_bb = pos.piece<c_my, pt>();
    Score score = 0;

    while (piece_bb > 0)
        score += strTable[NextSquare(piece_bb)];
    return score;
}

static Score
PieceTableStrengthMidGame(const ChessBoard& pos)
{
    Score pawns   = AddStrScore<WHITE, PAWN, wpBoard>(pos)
                  - AddStrScore<BLACK, PAWN, bpBoard>(pos);
    Score bishops = AddStrScore<WHITE, BISHOP, wBoard>(pos)
                  - AddStrScore<BLACK, BISHOP, bBoard>(pos);
    Score knights = AddStrScore<WHITE, KNIGHT, NBoard>(pos)
                  - AddStrScore<BLACK, KNIGHT, NBoard>(pos);
    Score rooks   = AddStrScore<WHITE, ROOK, wRBoard>(pos)
                  - AddStrScore<BLACK, ROOK, bRBoard>(pos);
    Score king    = AddStrScore<WHITE, KING, WhiteKingMidGameTable>(pos)
                  - AddStrScore<BLACK, KING, BlackKingMidGameTable>(pos);

    return pawns + bishops + knights + rooks + king;
}

template <Color c_my, PieceType pt>
static Score
AddMobilityScore(const ChessBoard& pos)
{
    Bitboard piece_bb = pos.piece<c_my, pt>();
    Bitboard occupied = pos.All();
    Bitboard squares  = 0;

    while (piece_bb > 0)
        squares |= AttackSquares<pt>(NextSquare(piece_bb), occupied);

    return PopCount(squares);
}

static Score
MobilityStrength(const ChessBoard& pos)
{
    Score bishops = AddMobilityScore<WHITE, BISHOP>(pos) - AddMobilityScore<BLACK, BISHOP>(pos);
    Score knights = AddMobilityScore<WHITE, KNIGHT>(pos) - AddMobilityScore<BLACK, KNIGHT>(pos);
    Score rooks   = AddMobilityScore<WHITE, ROOK  >(pos) - AddMobilityScore<BLACK, ROOK  >(pos);
    Score queens  = AddMobilityScore<WHITE, QUEEN >(pos) - AddMobilityScore<BLACK, QUEEN >(pos);

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
    for (int i = 0; i < 8; i++)
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
        int dist = (14 - Distance(pawn_sq, kpos)) - (14 - Distance(pawn_sq, ekpos));
        score += 6 * dist;
    }

    return score;
}

template<int debug = 0>
static Score
MidGameScore(const ChessBoard& pos, const EvalData& ed)
{
    Score materialScore   = MaterialDiffereceMidGame(pos);
    Score pieceTableScore = PieceTableStrengthMidGame(pos);
    Score mobilityScore   = MobilityStrength(pos);
    Score pawnStructure   = ed.pawnStructureScore;
    Score threatsScore    = Threats(pos);

    if (debug == 1)
    {
        cout << " -- MIDGAME -- " << endl;
        cout << "materialScore   = " << materialScore   << endl;
        cout << "pieceTableScore = " << pieceTableScore << endl;
        cout << "mobilityScore   = " << mobilityScore   << endl;
        cout << "threatsScore    = " << threatsScore    << endl << endl;
    }

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
MaterialDiffereceEndGame(const ChessBoard& pos)
{
    return PawnValueEg   * (pos.pieceCount<WHITE, PAWN  >() - pos.pieceCount<BLACK, PAWN  >())
		 + BishopValueEg * (pos.pieceCount<WHITE, BISHOP>() - pos.pieceCount<BLACK, BISHOP>())
		 + KnightValueEg * (pos.pieceCount<WHITE, KNIGHT>() - pos.pieceCount<BLACK, KNIGHT>())
		 + RookValueEg   * (pos.pieceCount<WHITE, ROOK  >() - pos.pieceCount<BLACK, ROOK  >())
		 + QueenValueEg  * (pos.pieceCount<WHITE, QUEEN >() - pos.pieceCount<BLACK, QUEEN >());
}

static Score
DistanceBetweenKingsScore(const ChessBoard& pos)
{
    Square wk_sq = SquareNo( pos.piece<WHITE, KING>() );
    Square bk_sq = SquareNo( pos.piece<BLACK, KING>() );

    int material_diff =
        + 3 * (pos.pieceCount<WHITE, BISHOP>() - pos.pieceCount<BLACK, BISHOP>())
        + 3 * (pos.pieceCount<WHITE, KNIGHT>() - pos.pieceCount<BLACK, KNIGHT>())
        + 5 * (pos.pieceCount<WHITE, ROOK  >() - pos.pieceCount<BLACK, ROOK  >())
        + 9 * (pos.pieceCount<WHITE, QUEEN >() - pos.pieceCount<BLACK, QUEEN >());

    int distance = 14 - Distance(wk_sq, bk_sq);
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
LoneKingEndGame(const ChessBoard& pos)
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

    // Color winningSide = (pos.pieceCount<WHITE, PAWN>() + ed.pieces[WHITE] > 0) ? WHITE : BLACK;
    constexpr Color losingSide  = ~winningSide;

    Square  wonKingSq = SquareNo( pos.piece<winningSide, KING>() );
    Square lostKingSq = SquareNo( pos.piece<losingSide , KING>() );

    Score winningSideCorrectionFactor = 2 * winningSide - 1;
    Score  losingSideCorrectionFactor = 2 *  losingSide - 1;

    Score distanceScore = DistanceBetweenKingsScore(pos);
    Score parityScore   = BishopColorCornerScore<winningSide>(pos);
    Score centreScore   = LongKingWinningEndGameTable[wonKingSq] * winningSideCorrectionFactor
                        + LoneKingLosingEndGameTable[lostKingSq] * losingSideCorrectionFactor;
    Score materialScore = MaterialDiffereceEndGame(pos);

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

template<int debug = 0>
static Score
EndGameScore(const ChessBoard& pos, const EvalData& ed)
{
    // Distance between kings
    // King in corners
    // BN endgames
    // Rule of Square (2n1k1r1/p7/3B1Rp1/2P2pKp/8/4P1P1/5P1P/8 w - - 17 45)

    Score materialScore   = MaterialDiffereceEndGame(pos);
    Score pieceTableScore = PieceTableStrengthEndGame(pos);
    Score pawnStructure   = ed.pawnStructureScore;
    Score distanceScore   = DistanceBetweenKingsScore(pos);
    Score parityScore     = ColorParityScore(pos);
    // Score threatsScore    = ed.threatScore;

    if (debug)
    {
        cout << " -- ENDGAME -- " << endl;
        cout << "materialScore   = " << materialScore   << endl;
        cout << "pieceTableScore = " << pieceTableScore << endl;
        cout << "distanceScore   = " << distanceScore   << endl;
        cout << "parityScore     = " << parityScore     << endl << endl;
    }

    float eval =
        ed.materialWeight     * float(materialScore)
      + ed.pieceTableWeight   * float(pieceTableScore)
      + ed.pawnSructureWeight * float(pawnStructure)
    //   + ed.threatsWeight      * float(threatsScore)
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
    if (ed.NoWhitePiecesOnBoard(pos) or ed.NoBlackPiecesOnBoard(pos))
        return (pos.pieceCount<WHITE, PAWN>() + ed.pieces[WHITE] > 0)
        ? LoneKingEndGame<WHITE>(pos) : LoneKingEndGame<BLACK>(pos);

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

    Score pawnStructrueWhite = PawnStructure<WHITE>(pos);
    Score pawnStructrueBlack = PawnStructure<BLACK>(pos);
    ed.pawnStructureScore = pawnStructrueWhite - pawnStructrueBlack;

    cout << "Score_pawn_structure_white = " << pawnStructrueWhite << endl;
    cout << "Score_pawn_structure_black = " << pawnStructrueBlack << endl;
    cout << "Score_pawn_structure_total = " << ed.pawnStructureScore << endl << endl;

    Score mgScore = MidGameScore<1>(pos, ed);
    Score egScore = EndGameScore<1>(pos, ed);

    Score score = Score( phase * float(mgScore) + (1 - phase) * float(egScore) );

    cout << "mg_score = " << mgScore << endl;
    cout << "eg_score = " << egScore << endl;
    cout << "score    = " << score   << endl;
    cout << "----------------------------------------------" << endl;

    return score;
}


Score
EvaluateThreats(const ChessBoard& pos)
{
    EvalData ed = EvalData(pos);
    cout << "--------------------------------------------------------" << endl;
    cout << "Phase = " << ed.phase << endl;

    Score attackValueWhite = AttackValue<WHITE>(pos);
    Score attackValueBlack = AttackValue<BLACK>(pos);

    Score distanceScoreWhite = AttackDistanceScore<WHITE>(pos);
    Score distanceScoreBlack = AttackDistanceScore<BLACK>(pos);

    Score kingMobilityWhite = KingMobilityScore<WHITE>(pos);
    Score kingMobilityBlack = KingMobilityScore<BLACK>(pos);

    Score openFileDeductionWhite = OpenFilesScore<WHITE>(pos);
    Score openFileDeductionBlack = OpenFilesScore<BLACK>(pos);

    Score attackersLeftWhite = AttackersLeft<WHITE>(pos);
    Score attackersLeftBlack = AttackersLeft<BLACK>(pos);

    Score defendersCountWhite = DefendersCount<WHITE>(pos);
    Score defendersCountBlack = DefendersCount<BLACK>(pos);

    Score lackOfSafetyWhite = (openFileDeductionWhite * (4 - kingMobilityWhite)) / (defendersCountWhite + 1);
    Score lackOfSafetyBlack = (openFileDeductionBlack * (4 - kingMobilityBlack)) / (defendersCountBlack + 1);

    Score currentAttackWhite = attackValueWhite * lackOfSafetyBlack + (distanceScoreWhite / (defendersCountBlack + 1));
    Score currentAttackBlack = attackValueBlack * lackOfSafetyWhite + (distanceScoreBlack / (defendersCountBlack + 1));

    Score longTermAttackWhite = ((attackersLeftWhite * attackersLeftWhite) + (openFileDeductionBlack * openFileDeductionBlack)) / (32 + defendersCountBlack);
    Score longTermAttackBlack = ((attackersLeftBlack * attackersLeftBlack) + (openFileDeductionWhite * openFileDeductionWhite)) / (32 + defendersCountWhite);

    cout << "attackValueWhite = " << attackValueWhite << endl;
    cout << "attackValueBlack = " << attackValueBlack << endl;
    cout << "distanceScoreWhite = " << distanceScoreWhite << endl;
    cout << "distanceScoreBlack = " << distanceScoreBlack << endl;
    cout << "kingMobilityWhite = " << kingMobilityWhite << endl;
    cout << "kingMobilityBlack = " << kingMobilityBlack << endl;
    cout << "openFileDeductionWhite = " << openFileDeductionWhite << endl;
    cout << "openFileDeductionBlack = " << openFileDeductionBlack << endl;
    cout << "attackersLeftWhite = " << attackersLeftWhite << endl;
    cout << "attackersLeftBlack = " << attackersLeftBlack << endl;
    cout << "defendersCountWhite = " << defendersCountWhite << endl;
    cout << "defendersCountBlack = " << defendersCountBlack << endl << endl;

    cout << "lackOfSafetyWhite = " << lackOfSafetyWhite << endl;
    cout << "lackOfSafetyBlack = " << lackOfSafetyBlack << endl;
    cout << "currentAttackWhite = " << currentAttackWhite << endl;
    cout << "currentAttackBlack = " << currentAttackBlack << endl;
    cout << "longTermAttackWhite = " << longTermAttackWhite << endl;
    cout << "longTermAttackBlack = " << longTermAttackBlack << endl;

    Score __x = (currentAttackWhite + longTermAttackWhite) - (currentAttackBlack + longTermAttackBlack);
    cout << "--------------------------------------------------------" << endl;
    return __x;
}
