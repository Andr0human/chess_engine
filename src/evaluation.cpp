
#include "evaluation.h"
#include "attacks.h"
#include "base_utils.h"
#include "types.h"

using std::abs;
using std::min;

EvalWeights evalWeights;

// Distance helpers from lookup_table.h. In the eval, `distance` means
// Manhattan distance.
using plt::chebyshevDistance;
using plt::manhattanDistance;

static int
distance(Square s, Square t)
{ return manhattanDistance(s, t); }


// ---------------------------------------------------------------------------
// Types used only inside evaluate(). The header only has what callers and the
// tuner need (EvalData, EvalWeights, EvalComponents).
// ---------------------------------------------------------------------------

// Fixed-point scale for the king safety terms.
//
// openFilesScore and lackOfSafety are ratios, and integer division rounds them
// down. For a castled king behind an unbroken pawn shield,
// `lackOfSafety = 2 * openFiles * (4 - kingMobility) / (defenders + 1)` is 4/9,
// which rounds to 0 and removes the whole attackValue * lackOfSafety term. So
// these values are multiplied by KS_SCALE and divided back once, at the end of
// threats().
//
// attackValue's `/ 4` still rounds down (see sideAttacks()). Apart from that,
// threatsScore has the same scale as without KS_SCALE, so the tuned
// threatsWeightMg still fits. The largest intermediate value is about 4e7,
// well inside int32.
constexpr Score KS_SCALE = 64;

// Attack sets for one side, built once per eval and used by king safety, king
// mobility and mobility. All three need the same attacks with the same
// occupancy, so one pass serves them all: 14 attack lookups with full material
// instead of about 56.
struct AttackInfo
{
  Bitboard bishop, knight, rook, queen;  // per piece type, for mobility
  Bitboard all;                          // the above plus pawns and king, for king mobility
  Score attackValue;                     // attacks near the enemy king, scaled by KS_SCALE
};

struct EvalAttacks
{
  AttackInfo side[COLOR_NB];  // indexed by Color (BLACK = 0, WHITE = 1)
};

// Piece-count differences (White minus Black), computed once. Used by the
// midgame and endgame material scores and the king distance term.
struct MaterialDiffs
{
  int pawn, bishop, knight, rook, queen;
};

// Terms used by both phases, so midGameScore() and endGameScore() compute them
// once.
struct SharedTerms
{
  MaterialDiffs material;
  TaperedScore  materialScore;
  TaperedScore  pieceSquare;
  int bishopPair;
  int isolated;
};

// Mobility per piece type, White minus Black. These are raw popcount
// differences; the EvalWeights values scale them. The eval and the tuner's
// cache both use this, so they always match.
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

// One pass over the pieces of type `pt`. Collects their attacks and scores the
// ones that reach the squares around the enemy king. kingOuterMasks is the outer
// ring without kingMasks and the king square (buildKingOuterTable in
// lookup_table.cpp), so the two rings don't overlap and one attack set can be
// tested against both.
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

  // The integer `/ 4` is kept before scaling by KS_SCALE. Without the rounding,
  // attackValue would grow by about 75%. It is multiplied by lackOfSafety (up to
  // about 260 for an exposed king), so threatsWeightMg would need retuning.
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
  // Used by all five calls below.
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
  // `(score / 4) + 1`, scaled by KS_SCALE. The 1<<(7-dist) values above are
  // still a step function. The scale only stops the division from rounding
  // them further.
  return (score * KS_SCALE) / 4 + KS_SCALE;
}

// `emyAttacks` is the other side's attacks, already built by sideAttacks().
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

  // attackValue* and openFileDeduction* are scaled by KS_SCALE. Everything below
  // stays scaled, and the scale is divided out once, in threatsScore.
  Score lackOfSafetyWhite = 2 * (openFileDeductionWhite * (4 - kingMobilityWhite)) / (defendersCountWhite + 1);
  Score lackOfSafetyBlack = 2 * (openFileDeductionBlack * (4 - kingMobilityBlack)) / (defendersCountBlack + 1);

  // attackValue * lackOfSafety is scaled by KS_SCALE^2, so divide by KS_SCALE
  // once. distanceScore isn't scaled, so multiply it by KS_SCALE before adding.
  Score currentAttackWhite = (attackValueWhite * lackOfSafetyBlack) / KS_SCALE + (distanceScoreWhite * KS_SCALE) / (defendersCountBlack + 1);
  Score currentAttackBlack = (attackValueBlack * lackOfSafetyWhite) / KS_SCALE + (distanceScoreBlack * KS_SCALE) / (defendersCountWhite + 1);

  // Same here: attackersLeft is a plain count, and openFileDeduction^2 is scaled
  // by KS_SCALE^2.
  Score longTermAttackWhite = ((attackersLeftWhite * attackersLeftWhite * KS_SCALE) + (openFileDeductionBlack * openFileDeductionBlack) / KS_SCALE) / (32 + defendersCountBlack);
  Score longTermAttackBlack = ((attackersLeftBlack * attackersLeftBlack * KS_SCALE) + (openFileDeductionWhite * openFileDeductionWhite) / KS_SCALE) / (32 + defendersCountWhite);

  Score threatsScore = ((currentAttackWhite + longTermAttackWhite) - (currentAttackBlack + longTermAttackBlack)) / KS_SCALE;

  if (debug)
  {
    // The king safety values are scaled by KS_SCALE. Print the real values, not
    // the scaled integers.
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

// The public version (evaluation.h), which builds the attack sets itself.
// evaluate() calls threatsImpl() directly so it can share them with mobility.
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

// Indexed by PieceType. The king carries no material.
static constexpr TaperedScore materialValue[] = {
  {},
  {  PawnValueMg,   PawnValueEg},
  {BishopValueMg, BishopValueEg},
  {KnightValueMg, KnightValueEg},
  {  RookValueMg,   RookValueEg},
  { QueenValueMg,  QueenValueEg},
  {},
};

static TaperedScore
taperedMaterial(const MaterialDiffs& md)
{
  return materialValue[PAWN  ] * md.pawn
       + materialValue[BISHOP] * md.bishop
       + materialValue[KNIGHT] * md.knight
       + materialValue[ROOK  ] * md.rook
       + materialValue[QUEEN ] * md.queen;
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

// The caller computes the board-wide inputs once and passes them in, instead
// of computing them for every pawn.
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
  Score materialScore   = shared.materialScore.mg;
  Score pieceTableScore = shared.pieceSquare.mg;
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
  // Pushes the losing king toward a corner, and distanceScore brings the
  // winning king closer for the mate. With bishop and knight, only the corners
  // of the bishop's colour are a forced win, so centreScore below adds a bonus
  // for those two corners.

  constexpr Color losingSide  = ~winningSide;

  Square lostKingSq = squareNo(pos.piece<losingSide , KING>());

  Score winningSideCorrectionFactor = 2 * winningSide - 1;
  Score  losingSideCorrectionFactor = 2 *  losingSide - 1;

  Score distanceScore = distanceBetweenKingsScore(pos, md);
  Score centreScore   = loneKingLosingEndGameTable[lostKingSq] * losingSideCorrectionFactor;
  Score materialScore = taperedMaterial(md).eg;

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

  // These are the same for every pawn, so compute them once.
  const Bitboard emyPawns = pos.piece<cEmy, PAWN>();
  const Square       kpos = squareNo(pos.piece< cMy, KING>());
  const Square      ekpos = squareNo(pos.piece<cEmy, KING>());
  const int emyKingToMove = cMy != pos.color;

  while (pawns != 0)
  {
    Square pawnSq = nextSquare(pawns);

    if (isPassedPawn<cMy>(emyPawns, pawnSq))
    {
      Score rankProgress = (7 * (cMy ^ 1)) + (pawnSq >> 3) * (2 * cMy - 1);
      score += 3 * rankProgress * rankProgress;

      if (canSafelyPromote<cMy>(ekpos, emyKingToMove, pawnSq))
      {
        Score reward = ed.pieces[cEmy] == 0 ? QueenValueEg : PawnValueEg >> 2;
        score += reward + 3 * rankProgress * rankProgress;
      }
    }

    // TODO: More points for being close to pawn which is closer to promotion
    int dist = (14 - distance(pawnSq, kpos)) - (14 - distance(pawnSq, ekpos));
    score += 6 * dist;
  }

  return score;
}

template<bool debug>
static Score
endGameScore(const ChessBoard& pos, const EvalData& ed, const SharedTerms& shared)
{
  Score materialScore   = shared.materialScore.eg;
  Score pieceTableScore = shared.pieceSquare.eg;
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

  // Computed once and shared. The attack sets are used by king safety and
  // mobility. Material, piece-square, bishop pair and isolated pawns are used by
  // both the midgame and the endgame score.
  const EvalAttacks atk = computeAttacks(pos);
  const SharedTerms shared = {
    material,
    taperedMaterial(material),
    pos.pieceSquare,
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

  // These special endgames skip the weighted eval (see evaluate()). Their score
  // doesn't use the weights, so they can't be tuned.
  if (pieceCount < 3)
  {
    if (pieceCount == 2
    and pos.count<PAWN  >() == 1
    and pos.count<BISHOP>() == 1
    and pos.count<WHITE, ALL>() == 1)
      return ec;  // minorPiecePawnEndgame, tunable = false
  }

  if ((pos.count<PAWN>() == 0) and (ed.pieces[WHITE] == 0 or ed.pieces[BLACK] == 0))
    return ec;  // loneKingEndGame, tunable = false

  ec.tunable = true;
  ec.phase   = phase;

  const MaterialDiffs material    = materialDiffs(pos);
  const TaperedScore  mat         = taperedMaterial(material);
  const TaperedScore  pieceSquare = pos.pieceSquare;
  const EvalAttacks   atk         = computeAttacks(pos);

  MobilityDiffs mob = mobilityDiffs(atk);

  ec.matMg     = float(mat.mg);
  ec.ptMg      = float(pieceSquare.mg);
  ec.mobBishop = mob.bishop;
  ec.mobKnight = mob.knight;
  ec.mobRook   = mob.rook;
  ec.mobQueen  = mob.queen;
  ec.threats   = float(threatsImpl<false>(pos, atk));

  ec.matEg    = float(mat.eg);
  ec.ptEg     = float(pieceSquare.eg);
  ec.pawnEg   = float(pawnStructureScoreEndgame<WHITE>(pos, ed)
              - pawnStructureScoreEndgame<BLACK>(pos, ed));
  ec.distance = float(distanceBetweenKingsScore(pos, material));

  const Bitboard allPawns = pos.piece<WHITE, PAWN>() | pos.piece<BLACK, PAWN>();

  ec.bishopPair = float(bishopPairDiff(pos));
  ec.rookFileMg = float(rookFileUnits<WHITE>(pos, allPawns) - rookFileUnits<BLACK>(pos, allPawns));
  ec.isolated   = float(isolatedPawnCount<WHITE>(pos) - isolatedPawnCount<BLACK>(pos));

  return ec;
}

PhaseSums
phaseSumsFromComponents(const EvalComponents& ec, const EvalWeights& w)
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

  return {mg, eg};
}

Score
evalFromComponents(const EvalComponents& ec, const EvalWeights& w)
{
  const PhaseSums sums = phaseSumsFromComponents(ec, w);

  // Match evaluate(): mg/eg are truncated to Score (int32) before the phase blend.
  Score mgScore = Score(sums.mg);
  Score egScore = Score(sums.eg);
  return Score( ec.phase * float(mgScore) + (1 - ec.phase) * float(egScore) );
}
