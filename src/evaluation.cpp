
#include "evaluation.h"
#include "attacks.h"
#include "base_utils.h"
#include "types.h"

using std::abs;
using std::min;

EvalWeights evalWeights;

static int
distance(Square s, Square t)
{
  int rank1 = s >> 3, file1 = s & 7;
  int rank2 = t >> 3, file2 = t & 7;

  return abs(rank1 - rank2) + abs(file1 - file2);
}

static int
chebyshevDistance(Square s1, Square s2)
{
  int rank1 = s1 >> 3, file1 = s1 & 7;
  int rank2 = s2 >> 3, file2 = s2 & 7;
  return std::max(abs(rank1 - rank2), abs(file1 - file2));
}

#ifndef THREATS

template <Color cMy, PieceType pt, const MaskTable& mask, Score increment>
static Score
attacksKing(const ChessBoard& pos)
{
  Score score = VALUE_ZERO;
  Bitboard pieceBb = pos.piece<cMy, pt>();
  Bitboard occupied = pos.all();
  Square kingSqEmy = squareNo(pos.piece<~cMy, KING>());

  while (pieceBb != 0)
  {
    Square sq = nextSquare(pieceBb);
    if ((attackSquares<pt>(sq, occupied) & mask[kingSqEmy]) != 0)
      score += increment;
  }

  return score;
}

template <Color cMy>
static Score
attackValue(const ChessBoard& pos)
{
  using plt::kingMasks;
  using plt::kingOuterMasks;

  Score attackValue = VALUE_ZERO;

  attackValue += attacksKing<cMy, KNIGHT, kingMasks, 2>(pos);
  attackValue += attacksKing<cMy, BISHOP, kingMasks, 3>(pos);
  attackValue += attacksKing<cMy, ROOK  , kingMasks, 4>(pos);
  attackValue += attacksKing<cMy, QUEEN , kingMasks, 6>(pos);

  attackValue += attacksKing<cMy, KNIGHT, kingOuterMasks, 1>(pos);
  attackValue += attacksKing<cMy, BISHOP, kingOuterMasks, 2>(pos);
  attackValue += attacksKing<cMy, ROOK  , kingOuterMasks, 3>(pos);
  attackValue += attacksKing<cMy, QUEEN , kingOuterMasks, 4>(pos);

  return attackValue / 4;
}


template <Color cMy, PieceType pt, int pieceVal>
static Score
calcDistanceScore(const ChessBoard& pos)
{
  Bitboard pieceBb = pos.piece<cMy, pt>();
  Square emyKingSq = squareNo(pos.piece<~cMy, KING>());

  Score score = VALUE_ZERO;

  while (pieceBb != 0)
    score += (pieceVal * (1 << (14 - distance(nextSquare(pieceBb), emyKingSq)))) >> 7;

  return score;
}

template <Color cMy>
static Score
attackDistanceScore(const ChessBoard& pos)
{
  Score distanceScore = VALUE_ZERO;

  distanceScore += calcDistanceScore<cMy, PAWN  , 1>(pos);
  distanceScore += calcDistanceScore<cMy, KNIGHT, 2>(pos);
  distanceScore += calcDistanceScore<cMy, BISHOP, 3>(pos);
  distanceScore += calcDistanceScore<cMy, ROOK  , 4>(pos);
  distanceScore += calcDistanceScore<cMy, QUEEN , 6>(pos);

  return distanceScore;
}

template <Color cMy>
static Score
openFilesScore(const ChessBoard& pos)
{
  Score score = VALUE_ZERO;
  Bitboard columnBb = FileA;

  int kingCol = squareNo(pos.piece<cMy, KING>()) & 7;
  Bitboard pawns = pos.piece<cMy, PAWN>();

  for (int col = 0; col < 8; col++)
  {
    if ((columnBb & pawns) == 0)
      score += (1 << (7 - abs(col - kingCol))) / 2;

    columnBb <<= 1;
  }
  return (score / 4) + 1;
}

template <Color cMy, PieceType pt>
static Bitboard
genAttackedSquares(const ChessBoard& pos)
{
  Bitboard squares = 0;
  Bitboard pieceBb = pos.piece<cMy, pt>();
  Bitboard occupied = pos.all();

  while (pieceBb != 0)
    squares |= attackSquares<pt>(nextSquare(pieceBb), occupied);
  return squares;
}

template <Color cMy>
static Score
kingMobilityScore(const ChessBoard& pos)
{
  const Color cEmy = ~cMy;
  Square kSq = squareNo(pos.piece<cMy, KING>());
  Bitboard piecesMy = pos.piece<cMy, ALL>();
  Bitboard attackedSquares = 0;

  attackedSquares |= pawnAttackSquares<cEmy>(pos);
  attackedSquares |= genAttackedSquares<cEmy, BISHOP>(pos);
  attackedSquares |= genAttackedSquares<cEmy, KNIGHT>(pos);
  attackedSquares |= genAttackedSquares<cEmy, ROOK>(pos);
  attackedSquares |= genAttackedSquares<cEmy, QUEEN>(pos);
  attackedSquares |= genAttackedSquares<cEmy, KING>(pos);

  int x = popCount(attackSquares<KING>(kSq, 0) & ~(piecesMy | attackedSquares));
  return min(x, 3);
}

template <Color cMy>
static Score
attackersLeft(const ChessBoard& pos)
{
  return Score(
    pos.count<cMy, KNIGHT>() + 2 * pos.count<cMy, BISHOP>()
    + 3 * pos.count<cMy, ROOK>() + 5 * pos.count<cMy, QUEEN>()
  );
}


template <Color cMy>
static Score
defendersCount(const ChessBoard& pos)
{
  Square kSq = squareNo(pos.piece<cMy, KING>());
  // Pawns in front of king
  Bitboard pawns = pos.piece<cMy, PAWN>();
  Bitboard mask = plt::pawnMasks[cMy][kSq] | plt::pawnCaptureMasks[cMy][kSq];

  Bitboard pieces = (pos.piece<cMy, BISHOP>() | pos.piece<cMy, KNIGHT>() | pos.piece<cMy, ROOK>())
      & (plt::kingMasks[kSq] | plt::kingOuterMasks[kSq]);

  return 2 * popCount(mask & pawns) + popCount(pieces);
}

template <bool debug>
Score
threats(const ChessBoard& pos)
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

  Score attackValueWhite = attackValue<WHITE>(pos);
  Score attackValueBlack = attackValue<BLACK>(pos);

  Score distanceScoreWhite = attackDistanceScore<WHITE>(pos);
  Score distanceScoreBlack = attackDistanceScore<BLACK>(pos);

  Score kingMobilityWhite = kingMobilityScore<WHITE>(pos);
  Score kingMobilityBlack = kingMobilityScore<BLACK>(pos);

  Score openFileDeductionWhite = openFilesScore<WHITE>(pos);
  Score openFileDeductionBlack = openFilesScore<BLACK>(pos);

  Score attackersLeftWhite = attackersLeft<WHITE>(pos);
  Score attackersLeftBlack = attackersLeft<BLACK>(pos);

  Score defendersCountWhite = defendersCount<WHITE>(pos);
  Score defendersCountBlack = defendersCount<BLACK>(pos);

  Score lackOfSafetyWhite = 2 * (openFileDeductionWhite * (4 - kingMobilityWhite)) / (defendersCountWhite + 1);
  Score lackOfSafetyBlack = 2 * (openFileDeductionBlack * (4 - kingMobilityBlack)) / (defendersCountBlack + 1);

  Score currentAttackWhite = attackValueWhite * lackOfSafetyBlack + (distanceScoreWhite / (defendersCountBlack + 1));
  Score currentAttackBlack = attackValueBlack * lackOfSafetyWhite + (distanceScoreBlack / (defendersCountWhite + 1));

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
materialDiffereceMidGame(const ChessBoard& pos)
{
  return PawnValueMg * (pos.count<WHITE, PAWN  >() - pos.count<BLACK, PAWN  >())
     + BishopValueMg * (pos.count<WHITE, BISHOP>() - pos.count<BLACK, BISHOP>())
     + KnightValueMg * (pos.count<WHITE, KNIGHT>() - pos.count<BLACK, KNIGHT>())
     +   RookValueMg * (pos.count<WHITE, ROOK  >() - pos.count<BLACK, ROOK  >())
     +  QueenValueMg * (pos.count<WHITE, QUEEN >() - pos.count<BLACK, QUEEN >());
}

template<Color cMy, PieceType pt, const ScoreTable& strTable>
static Score
addStrScore(const ChessBoard& pos)
{
  Bitboard pieceBb = pos.piece<cMy, pt>();
  Score score = 0;

  while (pieceBb > 0)
    score += strTable[nextSquare(pieceBb)];
  return score;
}

static Score
pieceTableStrengthMidGame(const ChessBoard& pos)
{
  Score pawns   = addStrScore<WHITE, PAWN , wpBoard>(pos)
                - addStrScore<BLACK, PAWN , bpBoard>(pos);
  Score bishops = addStrScore<WHITE, BISHOP, wBoard>(pos)
                - addStrScore<BLACK, BISHOP, bBoard>(pos);
  Score knights = addStrScore<WHITE, KNIGHT, NBoard>(pos)
                - addStrScore<BLACK, KNIGHT, NBoard>(pos);
  Score rooks   = addStrScore<WHITE, ROOK , wRBoard>(pos)
                - addStrScore<BLACK, ROOK , bRBoard>(pos);
  Score king    = addStrScore<WHITE, KING, whiteKingMidGameTable>(pos)
                - addStrScore<BLACK, KING, blackKingMidGameTable>(pos);

  return pawns + bishops + knights + rooks + king;
}

template <Color cMy, PieceType pt>
static Score
addMobilityScore(const ChessBoard& pos)
{
  Bitboard pieceBb = pos.piece<cMy, pt>();
  Bitboard occupied = pos.all();
  Bitboard squares  = 0;

  while (pieceBb > 0)
    squares |= attackSquares<pt>(nextSquare(pieceBb), occupied);

  return popCount(squares);
}

// White-relative per-piece-type mobility subtotals (raw popcount diffs, no per-piece
// scaling — the EvalWeights scalars do that). Used by both the live eval and the
// Texel cache so the two stay in lockstep.
struct MobilityDiffs
{
  float bishop, knight, rook, queen;

  float weighted(const EvalWeights& w) const
  {
    return w.mobBishopWeightMg * bishop
         + w.mobKnightWeightMg * knight
         + w.mobRookWeightMg   * rook
         + w.mobQueenWeightMg  * queen;
  }
};

static MobilityDiffs
mobilityDiffs(const ChessBoard& pos)
{
  return {
    float(addMobilityScore<WHITE, BISHOP>(pos) - addMobilityScore<BLACK, BISHOP>(pos)),
    float(addMobilityScore<WHITE, KNIGHT>(pos) - addMobilityScore<BLACK, KNIGHT>(pos)),
    float(addMobilityScore<WHITE, ROOK  >(pos) - addMobilityScore<BLACK, ROOK  >(pos)),
    float(addMobilityScore<WHITE, QUEEN >(pos) - addMobilityScore<BLACK, QUEEN >(pos)),
  };
}

template <Color cMy>
static bool
isPassedPawn(const ChessBoard& pos, Square pawnSq)
{
  const Color cEmy = ~cMy;
  const Bitboard emyPawns = pos.piece<cEmy, PAWN>();

  return (plt::passedPawnMasks[cMy][pawnSq] & emyPawns) == 0;
}

template <Color cMy>
static bool
canSafelyPromote(const ChessBoard& pos, Square pawnSq)
{
  Square emykingSq = squareNo(pos.piece<~cMy, KING>());
  Square   promoSq = Square(int(SQ_A8) * int(cMy)) + (pawnSq & 7);

  int emyKingToMove = cMy != pos.color;

  if (min(5, chebyshevDistance(pawnSq, promoSq)) < chebyshevDistance(emykingSq, promoSq) - emyKingToMove)
    return true;

  return false;
}

template <Color cMy>
static Score
pawnStructureScoreMidgame(const ChessBoard& pos)
{
  Bitboard  pawns = pos.piece<cMy , PAWN>();
  Bitboard column = FileA;
  Score score = 0;

  // Punish Double Pawns on same column
  for (int i = 0; i < 8; i++)
  {
    int p = popCount(column & pawns);
    score -= 36 * p * (p - 1);
    column <<= 1;
  }

  return score;
}

template<bool debug>
static Score
midGameScore(const ChessBoard& pos, float /*phase*/)
{
  Score materialScore   = materialDiffereceMidGame(pos);
  Score pieceTableScore = pieceTableStrengthMidGame(pos);
  MobilityDiffs mob     = mobilityDiffs(pos);
  Score pawnStructure   = pawnStructureScoreMidgame<WHITE>(pos)
                        - pawnStructureScoreMidgame<BLACK>(pos);
  Score threatsScore    = threats<debug>(pos);

  if (debug)
  {
    cout << "-------------------- MIDGAME --------------------\n"
      << "\nmaterialScore   = " << materialScore
      << "\npieceTableScore = " << pieceTableScore
      << "\nmobBishop       = " << mob.bishop
      << "\nmobKnight       = " << mob.knight
      << "\nmobRook         = " << mob.rook
      << "\nmobQueen        = " << mob.queen
      << "\npawnStructure   = " << pawnStructure
      << "\nthreatsScore    = " << threatsScore
      << "\n-------------------------------------------------" << endl;
  }

  float eval =
      evalWeights.materialWeightMg      * float(materialScore)
    + evalWeights.pieceTableWeightMg    * float(pieceTableScore)
    + mob.weighted(evalWeights)
    + evalWeights.pawnStructureWeightMg * float(pawnStructure)
    + evalWeights.threatsWeightMg       * float(threatsScore);

  return Score(eval);
}


#endif

#ifndef ENDGAME

static Score
materialDiffereceEndGame(const ChessBoard& pos)
{
  return PawnValueEg * (pos.count<WHITE, PAWN  >() - pos.count<BLACK, PAWN  >())
     + BishopValueEg * (pos.count<WHITE, BISHOP>() - pos.count<BLACK, BISHOP>())
     + KnightValueEg * (pos.count<WHITE, KNIGHT>() - pos.count<BLACK, KNIGHT>())
     +   RookValueEg * (pos.count<WHITE, ROOK  >() - pos.count<BLACK, ROOK  >())
     +  QueenValueEg * (pos.count<WHITE, QUEEN >() - pos.count<BLACK, QUEEN >());
}

static Score
distanceBetweenKingsScore(const ChessBoard& pos)
{
  Square wkSq = squareNo(pos.piece<WHITE, KING>());
  Square bkSq = squareNo(pos.piece<BLACK, KING>());

  int materialDiff =
    + 3 * (pos.count<WHITE, BISHOP>() - pos.count<BLACK, BISHOP>())
    + 3 * (pos.count<WHITE, KNIGHT>() - pos.count<BLACK, KNIGHT>())
    + 5 * (pos.count<WHITE, ROOK  >() - pos.count<BLACK, ROOK  >())
    + 9 * (pos.count<WHITE, QUEEN >() - pos.count<BLACK, QUEEN >());

  int dist = 14 - distance(wkSq, bkSq);
  Score score = (dist / 4) * (dist + 2) * materialDiff;
  return score;
}

template <Color winningSide, bool debug>
static Score
loneKingEndGame(const ChessBoard& pos)
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

  constexpr Color losingSide  = ~winningSide;

  Square lostKingSq = squareNo(pos.piece<losingSide , KING>());

  Score winningSideCorrectionFactor = 2 * winningSide - 1;
  Score  losingSideCorrectionFactor = 2 *  losingSide - 1;

  Score distanceScore = distanceBetweenKingsScore(pos);
  Score centreScore   = loneKingLosingEndGameTable[lostKingSq] * losingSideCorrectionFactor;
  Score materialScore = materialDiffereceEndGame(pos);

  if (pos.count<BISHOP>() == 1 and pos.count<KNIGHT>() == 1)
  {
    int isWhite = bool(pos.piece<winningSide, BISHOP>() & WhiteSquares);

    Score a = 14 - distance(lostKingSq, SQ_A1 + (7 * isWhite));
    Score b = 14 - distance(lostKingSq, SQ_H8 - (7 * isWhite));

    centreScore += (((1 << a) + (1 << b)) / 3) * winningSideCorrectionFactor;
  }

  Score score = materialScore + distanceScore + centreScore;

  if (debug)
  {
    cout << "winningSide = " << winningSide << endl;
    cout << "winningSideCorrectionFactor = " << winningSideCorrectionFactor << endl;
    cout << "losingSideCorrectionFactor  = " <<  losingSideCorrectionFactor << endl;

    cout << "MaterialScore = " << materialScore << endl;
    cout << "DistanceScore = " << distanceScore << endl;
    cout << "CentreScore   = " << centreScore   << endl;
    cout << "score         = " << score         << endl;
  }

  return score;
}

static Score
pieceTableStrengthEndGame(const ChessBoard& pos)
{
  const auto StrScore = [] (Bitboard piece, const ScoreTable& strTable)
  {
    Score score = 0;
    while (piece > 0)
      score += strTable[nextSquare(piece)];
    return score;
  };

  Score king = StrScore(pos.piece<WHITE, KING>(), kingEndGameTable)
             - StrScore(pos.piece<BLACK, KING>(), kingEndGameTable);
  return king;
}

template <Color cMy>
static Score
pawnStructureScoreEndgame(const ChessBoard& pos, const EvalData& ed)
{
  constexpr Color cEmy = ~cMy;
  Bitboard    pawns = pos.piece<cMy , PAWN>();
  Bitboard   column = FileA;
  Score score = 0;

  // Punish Double Pawns on same column
  for (int i = 0; i < 8; i++)
  {
    int p = popCount(column & pawns);
    score -= 52 * p * (p - 1);
    column <<= 1;
  }

  while (pawns != 0)
  {
    Square pawnSq = nextSquare(pawns);

    if (isPassedPawn<cMy>(pos, pawnSq))
    {
      // Reward for passed pawn
      Score rankProgress = (7 * (cMy ^ 1)) + (pawnSq >> 3) * (2 * cMy - 1);
      score += 3 * rankProgress * rankProgress;

      if (canSafelyPromote<cMy>(pos, pawnSq))
      {
        Score reward = ed.pieces[cEmy] == 0 ? QueenValueEg : PawnValueEg >> 2;
        score += reward + 3 * rankProgress * rankProgress;
      }
    }

    Square  kpos = squareNo(pos.piece< cMy, KING>());
    Square ekpos = squareNo(pos.piece<cEmy, KING>());

    // TODO: More points for being close to pawn which is closer to promotion
    // Add score for king close to passed pawn and
    // Reduce score if enemy king is close to pawn
    int dist = (14 - distance(pawnSq, kpos)) - (14 - distance(pawnSq, ekpos));
    score += 6 * dist;
  }

  return score;
}

template<bool debug>
static Score
endGameScore(const ChessBoard& pos, const EvalData& ed, float /*phase*/)
{
  // Distance between kings
  // King in corners
  // BN endgames
  // Rule of Square (2n1k1r1/p7/3B1Rp1/2P2pKp/8/4P1P1/5P1P/8 w - - 17 45)

  Score materialScore   = materialDiffereceEndGame(pos);
  Score pieceTableScore = pieceTableStrengthEndGame(pos);
  Score pawnStructure   = pawnStructureScoreEndgame<WHITE>(pos, ed)
                        - pawnStructureScoreEndgame<BLACK>(pos, ed);
  Score distanceScore   = distanceBetweenKingsScore(pos);

  if (debug)
  {
    cout << "-------------------- ENDGAME --------------------\n"
      << "\nmaterialScore      = " << materialScore
      << "\npieceTableScore    = " << pieceTableScore
      << "\npawnStructureScore = " << pawnStructure
      << "\ndistanceScore      = " << distanceScore
      << "\n-------------------------------------------------" << endl;
  }

  float eval =
      evalWeights.materialWeightEg      * float(materialScore)
    + evalWeights.pieceTableWeightEg    * float(pieceTableScore)
    + evalWeights.pawnStructureWeightEg * float(pawnStructure)
    + evalWeights.distanceWeightEg      * float(distanceScore);

  return Score(eval);
}

#endif

Score bishopPawnEndgame(const ChessBoard& pos)
{
  const Square pawnSq = squareNo(pos.piece<WHITE, PAWN>() | pos.piece<BLACK, PAWN>());
  const int row = pawnSq >> 3;
  return pos.piece<WHITE, PAWN>() ? (20 * row) : -(20 * (7 - row));
}

template <bool debug>
Score
evaluate(const ChessBoard& pos)
{
  EvalData ed = EvalData(pos);
  int side2move = 2 * int(pos.color) - 1;
  float phase = ed.phase;
  int pieceCount = pos.count<ALL>();

  if (pieceCount < 3)
  {
    if (pieceCount == 2
    and pos.count<PAWN  >() == 1
    and pos.count<BISHOP>() == 1
    and pos.count<WHITE, ALL>() == 1) return bishopPawnEndgame(pos) * side2move;
  }

  if (debug)
  {
    cout << "----------------------------------------------" << endl;
    cout << "BoardWeight = " << ed.boardWeight << endl;
    cout << "Phase = " << phase << endl;
  }

  // Special Piece EndGames
  if ((pos.count<PAWN>() == 0) and (ed.pieces[WHITE] == 0 or ed.pieces[BLACK] == 0))
  {
    Score score = (ed.pieces[WHITE] > 0) ? loneKingEndGame<WHITE, debug>(pos) : loneKingEndGame<BLACK, debug>(pos);
    return score * side2move;
  }

  Score mgScore = midGameScore<debug>(pos, phase);
  Score egScore = endGameScore<debug>(pos, ed, 1 - phase);

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


template Score threats<false>(const ChessBoard& pos);
template Score threats<true >(const ChessBoard& pos);

template Score evaluate<false>(const ChessBoard& pos);
template Score evaluate<true >(const ChessBoard& pos);


EvalComponents
extractEvalComponents(const ChessBoard& pos)
{
  EvalComponents ec;

  EvalData ed = EvalData(pos);
  float phase = ed.phase;
  int pieceCount = pos.count<ALL>();

  // Special endgames bypass the weighted eval entirely (see evaluate()): their score
  // does not depend on the 9 weights, so they are not tunable.
  if (pieceCount < 3)
  {
    if (pieceCount == 2
    and pos.count<PAWN  >() == 1
    and pos.count<BISHOP>() == 1
    and pos.count<WHITE, ALL>() == 1)
      return ec;  // bishopPawnEndgame, tunable = false
  }

  if ((pos.count<PAWN>() == 0) and (ed.pieces[WHITE] == 0 or ed.pieces[BLACK] == 0))
    return ec;  // loneKingEndGame, tunable = false

  ec.tunable = true;
  ec.phase   = phase;

  MobilityDiffs mob = mobilityDiffs(pos);

  ec.matMg     = float(materialDiffereceMidGame(pos));
  ec.ptMg      = float(pieceTableStrengthMidGame(pos));
  ec.mobBishop = mob.bishop;
  ec.mobKnight = mob.knight;
  ec.mobRook   = mob.rook;
  ec.mobQueen  = mob.queen;
  ec.pawnMg    = float(pawnStructureScoreMidgame<WHITE>(pos)
               - pawnStructureScoreMidgame<BLACK>(pos));
  ec.threats   = float(threats<false>(pos));

  ec.matEg    = float(materialDiffereceEndGame(pos));
  ec.ptEg     = float(pieceTableStrengthEndGame(pos));
  ec.pawnEg   = float(pawnStructureScoreEndgame<WHITE>(pos, ed)
              - pawnStructureScoreEndgame<BLACK>(pos, ed));
  ec.distance = float(distanceBetweenKingsScore(pos));

  return ec;
}

Score
evalFromComponents(const EvalComponents& ec, const EvalWeights& w)
{
  const MobilityDiffs mob{ec.mobBishop, ec.mobKnight, ec.mobRook, ec.mobQueen};

  float mg =
      w.materialWeightMg      * ec.matMg
    + w.pieceTableWeightMg    * ec.ptMg
    + mob.weighted(w)
    + w.pawnStructureWeightMg * ec.pawnMg
    + w.threatsWeightMg       * ec.threats;

  float eg =
      w.materialWeightEg      * ec.matEg
    + w.pieceTableWeightEg    * ec.ptEg
    + w.pawnStructureWeightEg * ec.pawnEg
    + w.distanceWeightEg      * ec.distance;

  // Match evaluate(): mg/eg are truncated to Score (int32) before the phase blend.
  Score mgScore = Score(mg);
  Score egScore = Score(eg);
  return Score( ec.phase * float(mgScore) + (1 - ec.phase) * float(egScore) );
}
