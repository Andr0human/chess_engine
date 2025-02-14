
#include "evaluation.h"
#include "attacks.h"
#include "base_utils.h"

using std::abs;
using std::min;

static int
Distance(Square s, Square t)
{
  int s_x = s & 7, s_y = s >> 3;
  int t_x = t & 7, t_y = t >> 3;

  return abs(s_x - t_x) + abs(s_y - t_y);
}

static int
ChebyshevDistance(Square s1, Square s2)
{
  int rank1 = s1 >> 3, file1 = s1 & 7;
  int rank2 = s2 >> 3, file2 = s2 & 7;
  return std::max(abs(rank1 - rank2), abs(file1 - file2));
}

#ifndef THREATS

template <Color c_my, PieceType pt, const MaskTable& mask, Score increment>
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

  while (piece_bb != 0)
    score += (pieceVal * (1 << (14 - Distance(NextSquare(piece_bb), emyKingSq)))) >> 7;

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
    if ((column_bb & pawns) == 0)
      score += (1 << (7 - abs(col - king_col))) / 2;

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
  const Color c_emy = ~c_my;
  Square k_sq = SquareNo(pos.piece<c_my, KING>());
  Bitboard pieces_my = pos.piece<c_my, ALL>();
  Bitboard attacked_squares = 0;

  attacked_squares |= PawnAttackSquares <c_emy>(pos);
  attacked_squares |= GenAttackedSquares<c_emy, BISHOP>(pos);
  attacked_squares |= GenAttackedSquares<c_emy, KNIGHT>(pos);
  attacked_squares |= GenAttackedSquares<c_emy, ROOK>(pos);
  attacked_squares |= GenAttackedSquares<c_emy, QUEEN>(pos);
  attacked_squares |= GenAttackedSquares<c_emy, KING>(pos);

  int __x = PopCount(AttackSquares<KING>(k_sq, 0) & ~(pieces_my | attacked_squares));
  return min(__x, 3);
}

template <Color c_my>
static Score
AttackersLeft(const ChessBoard& pos)
{
  return Score(
    pos.count<c_my, KNIGHT>() + 2 * pos.count<c_my, BISHOP>()
    + 3 * pos.count<c_my, ROOK>() + 5 * pos.count<c_my, QUEEN>()
  );
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

template <bool debug>
Score
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

  Score threatsScore = (currentAttackWhite + longTermAttackWhite) - (currentAttackBlack + longTermAttackBlack);

  if (debug)
  {
    cout << "-------------------- THREATS --------------------\n"
      << "\nattackValueWhite   = " << attackValueWhite
      << "\nattackValueBlack   = " << attackValueBlack
      << "\ndistanceScoreWhite = " << distanceScoreWhite
      << "\ndistanceScoreBlack = " << distanceScoreBlack
      << "\nkingMobilityWhite  = " << kingMobilityWhite
      << "\nkingMobilityBlack  = " << kingMobilityBlack
      << "\nopenFileDeductionWhite = " << openFileDeductionWhite
      << "\nopenFileDeductionBlack = " << openFileDeductionBlack
      << "\nattackersLeftWhite  = " << attackersLeftWhite
      << "\nattackersLeftBlack  = " << attackersLeftBlack
      << "\ndefendersCountWhite = " << defendersCountWhite
      << "\ndefendersCountBlack = " << defendersCountBlack << "\n"
      << "\nlackOfSafetyWhite   = " << lackOfSafetyWhite
      << "\nlackOfSafetyBlack   = " << lackOfSafetyBlack
      << "\ncurrentAttackWhite  = " << currentAttackWhite
      << "\ncurrentAttackBlack  = " << currentAttackBlack
      << "\nlongTermAttackWhite = " << longTermAttackWhite
      << "\nlongTermAttackBlack = " << longTermAttackBlack
      << "\n\nThreatsScore = " << threatsScore
      << "\n-------------------------------------------------" << endl;
  }

  return threatsScore;
}


#endif

#ifndef MIDGAME

static Score
MaterialDiffereceMidGame(const ChessBoard& pos)
{
  return PawnValueMg * (pos.count<WHITE, PAWN  >() - pos.count<BLACK, PAWN  >())
     + BishopValueMg * (pos.count<WHITE, BISHOP>() - pos.count<BLACK, BISHOP>())
     + KnightValueMg * (pos.count<WHITE, KNIGHT>() - pos.count<BLACK, KNIGHT>())
     +   RookValueMg * (pos.count<WHITE, ROOK  >() - pos.count<BLACK, ROOK  >())
     +  QueenValueMg * (pos.count<WHITE, QUEEN >() - pos.count<BLACK, QUEEN >());
}

template<Color c_my, PieceType pt, const ScoreTable& strTable>
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
  Score pawns   = AddStrScore<WHITE, PAWN , wpBoard>(pos)
                - AddStrScore<BLACK, PAWN , bpBoard>(pos);
  Score bishops = AddStrScore<WHITE, BISHOP, wBoard>(pos)
                - AddStrScore<BLACK, BISHOP, bBoard>(pos);
  Score knights = AddStrScore<WHITE, KNIGHT, NBoard>(pos)
                - AddStrScore<BLACK, KNIGHT, NBoard>(pos);
  Score rooks   = AddStrScore<WHITE, ROOK , wRBoard>(pos)
                - AddStrScore<BLACK, ROOK , bRBoard>(pos);
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
static bool
isPassedPawn(const ChessBoard& pos, Square pawnSq)
{
  const Color c_emy = ~c_my;
  const Bitboard emyPawns = pos.piece<c_emy, PAWN>();

  return (plt::PassedPawnMasks[c_my][pawnSq] & emyPawns) == 0;
}

template <Color c_my>
static bool
CanSafelyPromote(const ChessBoard& pos, Square pawnSq)
{
  Square emykingSq = SquareNo(pos.piece<~c_my, KING>());
  Square   promoSq = Square(int(SQ_A8) * int(c_my)) + (pawnSq & 7);

  int emyKingToMove = c_my != pos.color;

  if (min(5, ChebyshevDistance(pawnSq, promoSq)) < ChebyshevDistance(emykingSq, promoSq) - emyKingToMove)
    return true;

  return false;
}

template <Color c_my>
static Score
PawnStructure(const ChessBoard& pos, const EvalData& ed)
{
  const Color c_emy = ~c_my;
  Bitboard     pawns = pos.piece<c_my , PAWN>();
  Bitboard emy_pawns = pos.piece<c_emy, PAWN>();
  Bitboard    column = FileA;
  Score score = 0;

  // Punish Double Pawns on same column
  for (int i = 0; i < 8; i++)
  {
    int p = PopCount(column & pawns);
    score -= 36 * p * (p - 1);
    column <<= 1;
  }

  while (pawns != 0)
  {
    Square pawnSq = NextSquare(pawns);
    if ((plt::PassedPawnMasks[c_my][pawnSq] & emy_pawns) != 0)
      continue;

    if (isPassedPawn<c_my>(pos, pawnSq))
    {
      // Reward for passed pawn
      Score distToPromotion = (7 * (c_my ^ 1)) + (pawnSq >> 3) * (2 * c_my - 1);
      score += 3 * distToPromotion * distToPromotion;

      if (CanSafelyPromote<c_my>(pos, pawnSq))
      {
        Score reward = ed.pieces[c_emy] == 0 ? QueenValueEg : PawnValueEg >> 2;
        score += reward + 3 * distToPromotion * distToPromotion;
      }
    }

    Square  kpos = SquareNo(pos.piece<c_my , KING>());
    Square ekpos = SquareNo(pos.piece<c_emy, KING>());

    // TODO: More points for being close to pawn which is closer to promotion
    // Add score for king close to passed pawn and
    // Reduce score if enemy king is close to pawn
    int dist = (14 - Distance(pawnSq, kpos)) - (14 - Distance(pawnSq, ekpos));
    score += 6 * dist;
  }

  return score;
}

template<bool debug>
static Score
MidGameScore(const ChessBoard& pos, const EvalData& ed, float phase)
{
  if (phase < 0.2)
    return 0;

  Score materialScore   = MaterialDiffereceMidGame(pos);
  Score pieceTableScore = PieceTableStrengthMidGame(pos);
  Score mobilityScore   = MobilityStrength(pos);
  Score pawnStructure   = ed.pawnStructureScore;
  Score threatsScore    = Threats<debug>(pos);

  if (debug)
  {
    cout << "-------------------- MIDGAME --------------------\n"
      << "\nmaterialScore   = " << materialScore
      << "\npieceTableScore = " << pieceTableScore
      << "\nmobilityScore   = " << mobilityScore
      << "\nthreatsScore    = " << threatsScore
      << "\n-------------------------------------------------" << endl;
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
  return PawnValueEg * (pos.count<WHITE, PAWN  >() - pos.count<BLACK, PAWN  >())
     + BishopValueEg * (pos.count<WHITE, BISHOP>() - pos.count<BLACK, BISHOP>())
     + KnightValueEg * (pos.count<WHITE, KNIGHT>() - pos.count<BLACK, KNIGHT>())
     +   RookValueEg * (pos.count<WHITE, ROOK  >() - pos.count<BLACK, ROOK  >())
     +  QueenValueEg * (pos.count<WHITE, QUEEN >() - pos.count<BLACK, QUEEN >());
}

static Score
DistanceBetweenKingsScore(const ChessBoard& pos)
{
  Square wk_sq = SquareNo( pos.piece<WHITE, KING>() );
  Square bk_sq = SquareNo( pos.piece<BLACK, KING>() );

  int material_diff =
    + 3 * (pos.count<WHITE, BISHOP>() - pos.count<BLACK, BISHOP>())
    + 3 * (pos.count<WHITE, KNIGHT>() - pos.count<BLACK, KNIGHT>())
    + 5 * (pos.count<WHITE, ROOK  >() - pos.count<BLACK, ROOK  >())
    + 9 * (pos.count<WHITE, QUEEN >() - pos.count<BLACK, QUEEN >());

  int distance = 14 - Distance(wk_sq, bk_sq);
  Score score = (distance / 4) * (distance + 2) * material_diff;
  return score;
}

template <Color winningSide>
static Score
BishopColorCornerScore(const ChessBoard& pos)
{
  Bitboard bishops   = pos.piece<  winningSide, BISHOP>();
  Bitboard won_king  = pos.piece<  winningSide, KING  >();
  Bitboard lost_king = pos.piece< ~winningSide, KING  >();

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

template <Color winningSide, bool debug>
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
  Score centreScore   = LoneKingWinningEndGameTable[wonKingSq] * winningSideCorrectionFactor
                      + LoneKingLosingEndGameTable[lostKingSq] * losingSideCorrectionFactor;
  Score materialScore = MaterialDiffereceEndGame(pos);

  Score score = materialScore + distanceScore + parityScore + centreScore;

  if (debug)
  {
    cout << "winningSide = " << winningSide << endl;
    cout << "winningSideCorrectionFactor = " << winningSideCorrectionFactor << endl;
    cout << "losingSideCorrectionFactor  = " <<  losingSideCorrectionFactor << endl;

    cout << "centreScoreWinning = " << (LoneKingWinningEndGameTable[wonKingSq] * winningSideCorrectionFactor) << endl;
    cout << "centreScoreLosing  = " << (LoneKingLosingEndGameTable[lostKingSq] *  losingSideCorrectionFactor) << endl;

    cout << "MaterialScore = " << materialScore << endl;
    cout << "DistanceScore = " << distanceScore << endl;
    cout << "ParityScore   = " << parityScore   << endl;
    cout << "CentreScore   = " << centreScore   << endl;
    cout << "score         = " << score         << endl;
  }

  return score;
}

static Score
PieceTableStrengthEndGame(const ChessBoard& pos)
{
  const auto StrScore = [] (Bitboard piece, const ScoreTable& strTable)
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

template<bool debug>
static Score
EndGameScore(const ChessBoard& pos, const EvalData& ed, float phase)
{
  // Distance between kings
  // King in corners
  // BN endgames
  // Rule of Square (2n1k1r1/p7/3B1Rp1/2P2pKp/8/4P1P1/5P1P/8 w - - 17 45)

  if (phase < 0.2)
    return 0;

  Score materialScore   = MaterialDiffereceEndGame(pos);
  Score pieceTableScore = PieceTableStrengthEndGame(pos);
  Score pawnStructure   = ed.pawnStructureScore;
  Score distanceScore   = DistanceBetweenKingsScore(pos);
  Score parityScore     = ColorParityScore(pos);
  // Score threatsScore    = ed.threatScore;

  if (debug)
  {
    cout << "-------------------- ENDGAME --------------------\n"
      << "\nmaterialScore      = " << materialScore
      << "\npieceTableScore    = " << pieceTableScore
      << "\npawnStructureScore = " << pawnStructure
      << "\ndistanceScore      = " << distanceScore
      << "\nparityScore        = " << parityScore
      << "\n-------------------------------------------------" << endl;
  }

  float eval =
      ed.materialWeight     * float(materialScore)
    + ed.pieceTableWeight   * float(pieceTableScore)
    + 0.7f                  * float(pawnStructure)
    + float(distanceScore)
    + float(parityScore);

  return Score(eval);
}

#endif


template <bool debug>
Score
Evaluate(const ChessBoard& pos)
{
  EvalData ed = EvalData(pos);
  int side2move = 2 * int(pos.color) - 1;
  float phase = ed.phase;

  if (debug)
  {
    cout << "----------------------------------------------" << endl;
    cout << "BoardWeight = " << ed.boardWeight << endl;
    cout << "Phase = " << phase << endl;
  }

  // Special Piece EndGames
  if ((pos.piece<WHITE, PAWN>() + pos.piece<BLACK, PAWN>() == 0) and (ed.pieces[WHITE] == 0 or ed.pieces[BLACK] == 0))
  {
    Score score = (ed.pieces[WHITE] > 0) ? LoneKingEndGame<WHITE, debug>(pos) : LoneKingEndGame<BLACK, debug>(pos);
    return score * side2move;
  }

  Score pawnStructrueWhite = PawnStructure<WHITE>(pos, ed);
  Score pawnStructrueBlack = PawnStructure<BLACK>(pos, ed);
  ed.pawnStructureScore = pawnStructrueWhite - pawnStructrueBlack;

  if (debug)
  {
    cout << "Score_pawn_structure_white = " << pawnStructrueWhite << endl;
    cout << "Score_pawn_structure_black = " << pawnStructrueBlack << endl;
    cout << "Score_pawn_structure_total = " << ed.pawnStructureScore << endl << endl;
  }

  Score mgScore = MidGameScore<debug>(pos, ed,     phase);
  Score egScore = EndGameScore<debug>(pos, ed, 1 - phase);

  Score score = Score( phase * float(mgScore) + (1 - phase) * float(egScore) );

  if (debug)
  {
    cout << "mg_score = " << mgScore << endl;
    cout << "eg_score = " << egScore << endl;
    cout << "score    = " << score   << endl;
    cout << "----------------------------------------------" << endl;
  }

  return score * side2move;
}


template Score Threats<false>(const ChessBoard& pos);
template Score Threats<true >(const ChessBoard& pos);

template Score Evaluate<false>(const ChessBoard& pos);
template Score Evaluate<true >(const ChessBoard& pos);
