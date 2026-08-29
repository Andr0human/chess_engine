
#include "evaluation.h"
#include "attacks.h"
#include "base_utils.h"
#include "types.h"

using std::abs;
using std::min;

EvalWeights evalWeights;

// Table lookups -- see the metric note in lookup_table.h. `distance` is the
// evaluation's long-standing name for the manhattan metric.
using plt::chebyshevDistance;
using plt::manhattanDistance;

static int
distance(Square s, Square t)
{ return manhattanDistance(s, t); }


// ---------------------------------------------------------------------------
// Internal eval types. Implementation details of evaluate() -- the header exposes
// only what callers and the tuner need (EvalData / EvalWeights / EvalComponents).
// ---------------------------------------------------------------------------

// Fixed-point denominator for the king-safety chain.
//
// openFilesScore and lackOfSafety are ratios whose integer division truncates:
// `lackOfSafety = 2 * openFiles * (4 - kingMobility) / (defenders + 1)` evaluates to
// 4/9 for a castled king behind an intact pawn shield, i.e. 0, which would zero the
// whole `attackValue * lackOfSafety` channel -- every "my pieces are aimed at their
// king" term vanishing for exactly the positions where the attack is being built.
// Carrying those quantities in KS_SCALE-ths and dividing out once, at the end of
// threats(), keeps the chain continuous.
//
// attackValue's own `/ 4` is deliberately left truncating (see sideAttacks()): fixing
// it is a scale change, not a precision fix. With that one exception threatsScore
// keeps its original scale, so the tuned threatsWeightMg still applies. Worst-case
// intermediate is ~4e7, well inside int32.
constexpr Score KS_SCALE = 64;

// Per-colour attack summary, built once per eval and shared by king safety, king
// mobility and the mobility subtotals. All three consumers want the same attack sets
// over the same `pos.all()` occupancy, so one pass feeds them all: 14 magic lookups
// at full midgame material instead of the ~56 a per-consumer pass would cost.
struct AttackInfo
{
  Bitboard bishop, knight, rook, queen;  // per-type unions -- mobility scores each type
  Bitboard all;                          // ... plus pawns and king, for king mobility
  Score attackValue;                     // king-ring pressure, in KS_SCALE-ths
};

struct EvalAttacks
{
  AttackInfo side[COLOR_NB];  // indexed by Color (BLACK = 0, WHITE = 1)
};

// White-relative piece-count differences, computed once and consumed by the midgame
// material score, the endgame material score and the king-distance term.
struct MaterialDiffs
{
  int pawn, bishop, knight, rook, queen;
};

// Subtotals that BOTH phases consume, so midGameScore() and endGameScore() share one
// computation rather than each doing its own.
struct SharedTerms
{
  MaterialDiffs material;
  int bishopPair;
  int isolated;
};

// White-relative per-piece-type mobility subtotals (raw popcount diffs, no per-piece
// scaling -- the EvalWeights scalars do that). Used by both the live eval and the
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


#ifndef THREATS

// One pass over the pieces of type `pt`: unions their attack sets and scores king-ring
// pressure at the same time. kingOuterMasks is built as the outer ring with kingMasks
// and the king square removed (lookup_table.cpp, buildKingOuterTable), so the two rings
// are disjoint and one attack set can be tested against both.
template <Color cMy, PieceType pt, Score incInner, Score incOuter>
static Bitboard
collectAttacks(const ChessBoard& pos, Bitboard occupied, Square kingSqEmy, Score& ksValue)
{
  Bitboard squares = 0;
  Bitboard pieceBb = pos.piece<cMy, pt>();

  while (pieceBb != 0)
  {
    Bitboard attacks = attackSquares<pt>(nextSquare(pieceBb), occupied);
    squares |= attacks;

    if ((attacks & plt::kingMasks[kingSqEmy]) != 0)      ksValue += incInner;
    if ((attacks & plt::kingOuterMasks[kingSqEmy]) != 0) ksValue += incOuter;
  }

  return squares;
}

template <Color cMy>
static AttackInfo
sideAttacks(const ChessBoard& pos)
{
  const Bitboard occupied  = pos.all();
  const Square   kingSqMy  = squareNo(pos.piece< cMy, KING>());
  const Square   kingSqEmy = squareNo(pos.piece<~cMy, KING>());

  Score ksValue = VALUE_ZERO;
  AttackInfo info;

  info.knight = collectAttacks<cMy, KNIGHT, 2, 1>(pos, occupied, kingSqEmy, ksValue);
  info.bishop = collectAttacks<cMy, BISHOP, 3, 2>(pos, occupied, kingSqEmy, ksValue);
  info.rook   = collectAttacks<cMy, ROOK  , 4, 3>(pos, occupied, kingSqEmy, ksValue);
  info.queen  = collectAttacks<cMy, QUEEN , 6, 4>(pos, occupied, kingSqEmy, ksValue);

  info.all = info.knight | info.bishop | info.rook | info.queen
           | pawnAttackSquares<cMy>(pos)
           | attackSquares<KING>(kingSqMy, occupied);

  // Deliberately keeps the original integer `/ 4` before lifting into KS_SCALE-ths.
  // Dropping this truncation too is a scale change, not a precision fix: it inflates a
  // quantity that multiplies lackOfSafety (which reaches ~260 for an exposed king) by
  // ~75%, which the tuned threatsWeightMg no longer fits. It needs a retune first.
  info.attackValue = (ksValue / 4) * KS_SCALE;

  return info;
}

static EvalAttacks
computeAttacks(const ChessBoard& pos)
{
  EvalAttacks atk;
  atk.side[WHITE] = sideAttacks<WHITE>(pos);
  atk.side[BLACK] = sideAttacks<BLACK>(pos);
  return atk;
}


template <Color cMy, PieceType pt, int pieceVal>
static Score
calcDistanceScore(const ChessBoard& pos, Square emyKingSq)
{
  Bitboard pieceBb = pos.piece<cMy, pt>();

  Score score = VALUE_ZERO;

  while (pieceBb != 0)
    score += (pieceVal * (1 << (14 - distance(nextSquare(pieceBb), emyKingSq)))) >> 7;

  return score;
}

template <Color cMy>
static Score
attackDistanceScore(const ChessBoard& pos)
{
  // Hoisted: shared by all five calls below.
  const Square emyKingSq = squareNo(pos.piece<~cMy, KING>());
  Score distanceScore = VALUE_ZERO;

  distanceScore += calcDistanceScore<cMy, PAWN  , 1>(pos, emyKingSq);
  distanceScore += calcDistanceScore<cMy, KNIGHT, 2>(pos, emyKingSq);
  distanceScore += calcDistanceScore<cMy, BISHOP, 3>(pos, emyKingSq);
  distanceScore += calcDistanceScore<cMy, ROOK  , 4>(pos, emyKingSq);
  distanceScore += calcDistanceScore<cMy, QUEEN , 6>(pos, emyKingSq);

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
  // KS_SCALE-ths of the original `(score / 4) + 1`. The 1<<(7-dist) ramp above is
  // still a step function -- smoothing its shape is a separate change; this only
  // stops the divide from quantising it further.
  return (score * KS_SCALE) / 4 + KS_SCALE;
}

// `emyAttacks` is the opposing colour attack union, already built by sideAttacks().
template <Color cMy>
static Score
kingMobilityScore(const ChessBoard& pos, const AttackInfo& emyAttacks)
{
  Square kSq = squareNo(pos.piece<cMy, KING>());
  Bitboard piecesMy = pos.piece<cMy, ALL>();

  int x = popCount(attackSquares<KING>(kSq, 0) & ~(piecesMy | emyAttacks.all));
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
static Score
threatsImpl(const ChessBoard& pos, const EvalAttacks& atk)
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

  Score attackValueWhite = atk.side[WHITE].attackValue;
  Score attackValueBlack = atk.side[BLACK].attackValue;

  Score distanceScoreWhite = attackDistanceScore<WHITE>(pos);
  Score distanceScoreBlack = attackDistanceScore<BLACK>(pos);

  Score kingMobilityWhite = kingMobilityScore<WHITE>(pos, atk.side[BLACK]);
  Score kingMobilityBlack = kingMobilityScore<BLACK>(pos, atk.side[WHITE]);

  Score openFileDeductionWhite = openFilesScore<WHITE>(pos);
  Score openFileDeductionBlack = openFilesScore<BLACK>(pos);

  Score attackersLeftWhite = attackersLeft<WHITE>(pos);
  Score attackersLeftBlack = attackersLeft<BLACK>(pos);

  Score defendersCountWhite = defendersCount<WHITE>(pos);
  Score defendersCountBlack = defendersCount<BLACK>(pos);

  // attackValue* and openFileDeduction* arrive in KS_SCALE-ths; everything below stays
  // in KS_SCALE-ths and is divided out once, at threatsScore.
  Score lackOfSafetyWhite = 2 * (openFileDeductionWhite * (4 - kingMobilityWhite)) / (defendersCountWhite + 1);
  Score lackOfSafetyBlack = 2 * (openFileDeductionBlack * (4 - kingMobilityBlack)) / (defendersCountBlack + 1);

  // attackValue * lackOfSafety is KS_SCALE^2; distanceScore is unscaled and has to be
  // lifted into KS_SCALE-ths to be added to it.
  Score currentAttackWhite = (attackValueWhite * lackOfSafetyBlack) / KS_SCALE + (distanceScoreWhite * KS_SCALE) / (defendersCountBlack + 1);
  Score currentAttackBlack = (attackValueBlack * lackOfSafetyWhite) / KS_SCALE + (distanceScoreBlack * KS_SCALE) / (defendersCountWhite + 1);

  // Likewise: attackersLeft is a plain count, openFileDeduction^2 is KS_SCALE^2.
  Score longTermAttackWhite = ((attackersLeftWhite * attackersLeftWhite * KS_SCALE) + (openFileDeductionBlack * openFileDeductionBlack) / KS_SCALE) / (32 + defendersCountBlack);
  Score longTermAttackBlack = ((attackersLeftBlack * attackersLeftBlack * KS_SCALE) + (openFileDeductionWhite * openFileDeductionWhite) / KS_SCALE) / (32 + defendersCountWhite);

  Score threatsScore = ((currentAttackWhite + longTermAttackWhite) - (currentAttackBlack + longTermAttackBlack)) / KS_SCALE;

  if (debug)
  {
    // The king-safety chain runs in KS_SCALE-ths (see KS_SCALE); print the values it
    // actually represents, not the raw fixed-point integers.
    const auto ks = [](Score v) { return double(v) / double(KS_SCALE); };

    cout << "-------------------- THREATS --------------------\n"
      << "\nattackValueWhite   = " << ks(attackValueWhite)
      << "\nattackValueBlack   = " << ks(attackValueBlack)
      << "\ndistanceScoreWhite = " << distanceScoreWhite
      << "\ndistanceScoreBlack = " << distanceScoreBlack
      << "\nkingMobilityWhite  = " << kingMobilityWhite
      << "\nkingMobilityBlack  = " << kingMobilityBlack
      << "\nopenFileDeductionWhite = " << ks(openFileDeductionWhite)
      << "\nopenFileDeductionBlack = " << ks(openFileDeductionBlack)
      << "\nattackersLeftWhite  = " << attackersLeftWhite
      << "\nattackersLeftBlack  = " << attackersLeftBlack
      << "\ndefendersCountWhite = " << defendersCountWhite
      << "\ndefendersCountBlack = " << defendersCountBlack << "\n"
      << "\nlackOfSafetyWhite   = " << ks(lackOfSafetyWhite)
      << "\nlackOfSafetyBlack   = " << ks(lackOfSafetyBlack)
      << "\ncurrentAttackWhite  = " << ks(currentAttackWhite)
      << "\ncurrentAttackBlack  = " << ks(currentAttackBlack)
      << "\nlongTermAttackWhite = " << ks(longTermAttackWhite)
      << "\nlongTermAttackBlack = " << ks(longTermAttackBlack)
      << "\n\nThreatsScore = " << threatsScore
      << "\n-------------------------------------------------" << endl;
  }

  return threatsScore;
}

// Public entry point (evaluation.h): builds the attack maps itself. evaluate() calls
// threatsImpl() directly so it can share the maps with mobility and king safety.
template <bool debug>
Score
threats(const ChessBoard& pos)
{ return threatsImpl<debug>(pos, computeAttacks(pos)); }


#endif

#ifndef MIDGAME

// Smear a bitboard so every occupied file is fully set (standard file-fill).
static Bitboard
fileFill(Bitboard b)
{
  b |= b >> 8;  b |= b << 8;
  b |= b >> 16; b |= b << 16;
  b |= b >> 32; b |= b << 32;
  return b;
}

// +1 if White has the bishop pair, -1 if Black does, 0 otherwise.
static int
bishopPairDiff(const ChessBoard& pos)
{
  return int(pos.count<WHITE, BISHOP>() >= 2)
       - int(pos.count<BLACK, BISHOP>() >= 2);
}

// Rook-file "units": +2 per rook on a fully-open file, +1 per semi-open file.
template <Color cMy>
static Score
rookFileUnits(const ChessBoard& pos, Bitboard allPawns)
{
  Bitboard rooks    = pos.piece<cMy, ROOK>();
  Bitboard myPawns  = pos.piece<cMy, PAWN>();
  Score units = 0;

  while (rooks != 0)
  {
    Square sq = nextSquare(rooks);
    Bitboard file = FileA << (sq & 7);

    if      ((file & allPawns) == 0) units += 2;  // open
    else if ((file & myPawns)  == 0) units += 1;  // semi-open
  }

  return units;
}

// Count of pawns with no friendly pawn on either adjacent file.
template <Color cMy>
static int
isolatedPawnCount(const ChessBoard& pos)
{
  Bitboard pawns     = pos.piece<cMy, PAWN>();
  Bitboard files     = fileFill(pawns);
  Bitboard neighbors = ((files & ~Bitboard(FileH)) << 1)
                     | ((files & ~Bitboard(FileA)) >> 1);
  return popCount(pawns & ~neighbors);
}

static MaterialDiffs
materialDiffs(const ChessBoard& pos)
{
  return {
    pos.count<WHITE, PAWN  >() - pos.count<BLACK, PAWN  >(),
    pos.count<WHITE, BISHOP>() - pos.count<BLACK, BISHOP>(),
    pos.count<WHITE, KNIGHT>() - pos.count<BLACK, KNIGHT>(),
    pos.count<WHITE, ROOK  >() - pos.count<BLACK, ROOK  >(),
    pos.count<WHITE, QUEEN >() - pos.count<BLACK, QUEEN >(),
  };
}

static Score
materialDiffereceMidGame(const MaterialDiffs& md)
{
  return PawnValueMg * md.pawn
     + BishopValueMg * md.bishop
     + KnightValueMg * md.knight
     +   RookValueMg * md.rook
     +  QueenValueMg * md.queen;
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

// Reads the per-type attack unions built by sideAttacks().
static MobilityDiffs
mobilityDiffs(const EvalAttacks& atk)
{
  const AttackInfo& w = atk.side[WHITE];
  const AttackInfo& b = atk.side[BLACK];

  return {
    float(popCount(w.bishop) - popCount(b.bishop)),
    float(popCount(w.knight) - popCount(b.knight)),
    float(popCount(w.rook  ) - popCount(b.rook  )),
    float(popCount(w.queen ) - popCount(b.queen )),
  };
}

// Board-wide inputs are hoisted above the caller pawn loop and passed in, rather
// than re-derived for every pawn.
template <Color cMy>
static bool
isPassedPawn(Bitboard emyPawns, Square pawnSq)
{
  return (plt::passedPawnMasks[cMy][pawnSq] & emyPawns) == 0;
}

template <Color cMy>
static bool
canSafelyPromote(Square emykingSq, int emyKingToMove, Square pawnSq)
{
  Square promoSq = Square(int(SQ_A8) * int(cMy)) + (pawnSq & 7);

  if (min(5, chebyshevDistance(pawnSq, promoSq)) < chebyshevDistance(emykingSq, promoSq) - emyKingToMove)
    return true;

  return false;
}

template<bool debug>
static Score
midGameScore(const ChessBoard& pos, const EvalAttacks& atk, const SharedTerms& shared)
{
  Score materialScore   = materialDiffereceMidGame(shared.material);
  Score pieceTableScore = pieceTableStrengthMidGame(pos);
  MobilityDiffs mob     = mobilityDiffs(atk);
  Score threatsScore    = threatsImpl<debug>(pos, atk);

  const Bitboard allPawns = pos.piece<WHITE, PAWN>() | pos.piece<BLACK, PAWN>();

  int   bishopPair = shared.bishopPair;
  Score rookFile   = rookFileUnits<WHITE>(pos, allPawns) - rookFileUnits<BLACK>(pos, allPawns);
  int   isolated   = shared.isolated;

  if (debug)
  {
    cout << "-------------------- MIDGAME --------------------\n"
      << "\nmaterialScore   = " << materialScore
      << "\npieceTableScore = " << pieceTableScore
      << "\nmobBishop       = " << mob.bishop
      << "\nmobKnight       = " << mob.knight
      << "\nmobRook         = " << mob.rook
      << "\nmobQueen        = " << mob.queen
      << "\nthreatsScore    = " << threatsScore
      << "\nbishopPair      = " << bishopPair
      << "\nrookFile        = " << rookFile
      << "\nisolated        = " << isolated
      << "\n-------------------------------------------------" << endl;
  }

  float eval =
      evalWeights.materialWeightMg      * float(materialScore)
    + evalWeights.pieceTableWeightMg    * float(pieceTableScore)
    + mob.weighted(evalWeights)
    + evalWeights.threatsWeightMg       * float(threatsScore)
    + evalWeights.bishopPairWeightMg    * float(bishopPair)
    + evalWeights.rookFileWeightMg      * float(rookFile)
    + evalWeights.isolatedPawnWeightMg  * float(isolated);

  return Score(eval);
}


#endif

#ifndef ENDGAME

static Score
materialDiffereceEndGame(const MaterialDiffs& md)
{
  return PawnValueEg * md.pawn
     + BishopValueEg * md.bishop
     + KnightValueEg * md.knight
     +   RookValueEg * md.rook
     +  QueenValueEg * md.queen;
}

static Score
distanceBetweenKingsScore(const ChessBoard& pos, const MaterialDiffs& md)
{
  Square wkSq = squareNo(pos.piece<WHITE, KING>());
  Square bkSq = squareNo(pos.piece<BLACK, KING>());

  int materialDiff =
    + 3 * md.bishop
    + 3 * md.knight
    + 5 * md.rook
    + 9 * md.queen;

  int dist = 14 - distance(wkSq, bkSq);
  Score score = (dist / 4) * (dist + 2) * materialDiff;
  return score;
}

template <Color winningSide, bool debug>
static Score
loneKingEndGame(const ChessBoard& pos, const MaterialDiffs& md)
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

  Score distanceScore = distanceBetweenKingsScore(pos, md);
  Score centreScore   = loneKingLosingEndGameTable[lostKingSq] * losingSideCorrectionFactor;
  Score materialScore = materialDiffereceEndGame(md);

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

  // Loop invariants: neither these nor the passed-pawn helper inputs vary per pawn.
  const Bitboard emyPawns = pos.piece<cEmy, PAWN>();
  const Square       kpos = squareNo(pos.piece< cMy, KING>());
  const Square      ekpos = squareNo(pos.piece<cEmy, KING>());
  const int emyKingToMove = cMy != pos.color;

  while (pawns != 0)
  {
    Square pawnSq = nextSquare(pawns);

    if (isPassedPawn<cMy>(emyPawns, pawnSq))
    {
      // Reward for passed pawn
      Score rankProgress = (7 * (cMy ^ 1)) + (pawnSq >> 3) * (2 * cMy - 1);
      score += 3 * rankProgress * rankProgress;

      if (canSafelyPromote<cMy>(ekpos, emyKingToMove, pawnSq))
      {
        Score reward = ed.pieces[cEmy] == 0 ? QueenValueEg : PawnValueEg >> 2;
        score += reward + 3 * rankProgress * rankProgress;
      }
    }

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
endGameScore(const ChessBoard& pos, const EvalData& ed, const SharedTerms& shared)
{
  // Distance between kings
  // King in corners
  // BN endgames
  // Rule of Square (2n1k1r1/p7/3B1Rp1/2P2pKp/8/4P1P1/5P1P/8 w - - 17 45)

  Score materialScore   = materialDiffereceEndGame(shared.material);
  Score pieceTableScore = pieceTableStrengthEndGame(pos);
  Score pawnStructure   = pawnStructureScoreEndgame<WHITE>(pos, ed)
                        - pawnStructureScoreEndgame<BLACK>(pos, ed);
  Score distanceScore   = distanceBetweenKingsScore(pos, shared.material);

  int bishopPair = shared.bishopPair;
  int isolated   = shared.isolated;

  if (debug)
  {
    cout << "-------------------- ENDGAME --------------------\n"
      << "\nmaterialScore      = " << materialScore
      << "\npieceTableScore    = " << pieceTableScore
      << "\npawnStructureScore = " << pawnStructure
      << "\ndistanceScore      = " << distanceScore
      << "\nbishopPair         = " << bishopPair
      << "\nisolated           = " << isolated
      << "\n-------------------------------------------------" << endl;
  }

  float eval =
      evalWeights.materialWeightEg      * float(materialScore)
    + evalWeights.pieceTableWeightEg    * float(pieceTableScore)
    + evalWeights.pawnStructureWeightEg * float(pawnStructure)
    + evalWeights.distanceWeightEg      * float(distanceScore)
    + evalWeights.bishopPairWeightEg    * float(bishopPair)
    + evalWeights.isolatedPawnWeightEg  * float(isolated);

  return Score(eval);
}

#endif

Score minorPiecePawnEndgame(const ChessBoard& pos)
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
    if ((pieceCount == 2) and
        (pos.count<PAWN  >() == 1) and
        (pos.count<BISHOP>() == 1 or pos.count<KNIGHT>() == 1) and
        (pos.count<WHITE, ALL>() == 1)
    ) return minorPiecePawnEndgame(pos) * side2move;
  }

  if (debug)
  {
    cout << "----------------------------------------------" << endl;
    cout << "BoardWeight = " << ed.boardWeight << endl;
    cout << "Phase = " << phase << endl;
  }

  const MaterialDiffs material = materialDiffs(pos);

  // Special Piece EndGames
  if ((pos.count<PAWN>() == 0) and (ed.pieces[WHITE] == 0 or ed.pieces[BLACK] == 0))
  {
    Score score = (ed.pieces[WHITE] > 0)
      ? loneKingEndGame<WHITE, debug>(pos, material)
      : loneKingEndGame<BLACK, debug>(pos, material);
    return score * side2move;
  }

  // Built once and shared: the attack maps feed king safety AND mobility; bishopPair
  // and isolated feed both the midgame and the endgame subscore.
  const EvalAttacks atk = computeAttacks(pos);
  const SharedTerms shared = {
    material,
    bishopPairDiff(pos),
    isolatedPawnCount<WHITE>(pos) - isolatedPawnCount<BLACK>(pos)
  };

  Score mgScore = midGameScore<debug>(pos, atk, shared);
  Score egScore = endGameScore<debug>(pos, ed, shared);

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

  const MaterialDiffs material = materialDiffs(pos);
  const EvalAttacks   atk      = computeAttacks(pos);

  MobilityDiffs mob = mobilityDiffs(atk);

  ec.matMg     = float(materialDiffereceMidGame(material));
  ec.ptMg      = float(pieceTableStrengthMidGame(pos));
  ec.mobBishop = mob.bishop;
  ec.mobKnight = mob.knight;
  ec.mobRook   = mob.rook;
  ec.mobQueen  = mob.queen;
  ec.threats   = float(threatsImpl<false>(pos, atk));

  ec.matEg    = float(materialDiffereceEndGame(material));
  ec.ptEg     = float(pieceTableStrengthEndGame(pos));
  ec.pawnEg   = float(pawnStructureScoreEndgame<WHITE>(pos, ed)
              - pawnStructureScoreEndgame<BLACK>(pos, ed));
  ec.distance = float(distanceBetweenKingsScore(pos, material));

  const Bitboard allPawns = pos.piece<WHITE, PAWN>() | pos.piece<BLACK, PAWN>();

  ec.bishopPair = float(bishopPairDiff(pos));
  ec.rookFileMg = float(rookFileUnits<WHITE>(pos, allPawns) - rookFileUnits<BLACK>(pos, allPawns));
  ec.isolated   = float(isolatedPawnCount<WHITE>(pos) - isolatedPawnCount<BLACK>(pos));

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
    + w.threatsWeightMg       * ec.threats
    + w.bishopPairWeightMg    * ec.bishopPair
    + w.rookFileWeightMg      * ec.rookFileMg
    + w.isolatedPawnWeightMg  * ec.isolated;

  float eg =
      w.materialWeightEg      * ec.matEg
    + w.pieceTableWeightEg    * ec.ptEg
    + w.pawnStructureWeightEg * ec.pawnEg
    + w.distanceWeightEg      * ec.distance
    + w.bishopPairWeightEg    * ec.bishopPair
    + w.isolatedPawnWeightEg  * ec.isolated;

  // Match evaluate(): mg/eg are truncated to Score (int32) before the phase blend.
  Score mgScore = Score(mg);
  Score egScore = Score(eg);
  return Score( ec.phase * float(mgScore) + (1 - ec.phase) * float(egScore) );
}
