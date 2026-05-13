
#include <array>
#include <chrono>
#include <utility>
#include "attacks.h"
#include "movegen.h"


#ifndef MOVEGEN_UTILS

bool
isLegalMoveForPosition(Move move, const ChessBoard& pos)
{
  Square ip = Square(move & 63);
  Square fp = Square((move >> 6) & 63);

  Piece ipt = pos.pieceOnSquare(ip);
  Piece fpt = pos.pieceOnSquare(fp);

  Color side = pos.color;

  // If no piece on init_sq or piece_color != side_to_move
  if (((ipt & 7) == 0) or (((ipt >> 3) & 1) != side))
    return false;

  // If capt_piece_color == side_to_move
  if ((fpt != 0) and ((fpt >> 3) & 1) == side)
    return false;

  const MoveList myMoves = generateMoves(pos);

  if ((type_of(ipt) != PAWN)) {
    return ((1ULL << ip) & myMoves.initSquares)
        && ((1ULL << fp) & myMoves.destSquares[ip]) != 0;
  }

  MoveArray movesArray;
  myMoves.getMoves(pos, movesArray);

  for (const Move legalMove : movesArray)
    if (filter(move) == filter(legalMove)) return true;

  return false;
}


#endif

#ifndef Piece_Movement

#ifndef PAWNS

template <Color cMy>
static bool
enpassantRecheck(Square ip, const ChessBoard& pos)
{
  constexpr Color cEmy = ~cMy;

  Square kpos = squareNo( pos.piece<cMy, KING>() );
  Square eps = pos.enPassantSquare();

  Bitboard erq = pos.piece<cEmy, QUEEN>() | pos.piece<cEmy, ROOK>();
  Bitboard ebq = pos.piece<cEmy, QUEEN>() | pos.piece<cEmy, BISHOP>();
  Bitboard  ap = pos.all() ^ ((1ULL << ip) | (1ULL << (eps - 8 * (2 * cMy - 1))));

  Bitboard res  = msb((plt::leftMasks[kpos] & ap)) | lsb((plt::rightMasks[kpos] & ap));
  Bitboard tmp1 = msb((plt::downLeftMasks[kpos] & ap)) | lsb((plt::upRightMasks[kpos] & ap));
  Bitboard tmp2 = lsb((plt::upLeftMasks[kpos] & ap)) | msb((plt::downRightMasks[kpos] & ap));

  if (res & erq) return false;
  // Board has to be invalid for this check
  if ((tmp1 | tmp2) & ebq) return false;

  return true;
}

template <Color cMy, ShifterFunc shift>
static inline void
enpassantPawns(const ChessBoard& pos, MoveList& myMoves,
  Bitboard lPawns, Bitboard rPawns, int KA)
{
  constexpr Color cEmy = ~cMy;
  constexpr int own =  cMy << 3;
  constexpr int emy = cEmy << 3;

  Square kpos = squareNo( pos.piece<cMy, KING>() );
  Square eps  = pos.enPassantSquare();
  Bitboard ep = 1ULL << eps;

  if (eps == SQUARE_NB || (KA == 1 and !(plt::pawnCaptureMasks[cMy][kpos] & pos.piece<cEmy, PAWN>())))
    return;

  if ((shift(lPawns, 7 + (own >> 2)) & ep) and enpassantRecheck<cMy>(eps + (2 * emy - 9), pos))
    myMoves.enpassantPawns |= 1ULL << (eps + (2 * emy - 9));

  if ((shift(rPawns, 7 + (emy >> 2)) & ep) and enpassantRecheck<cMy>(eps + (2 * emy - 7), pos))
    myMoves.enpassantPawns |= 1ULL << (eps + (2 * emy - 7));
}

template <Color cMy>
static inline void
promotionPawns(MoveList &myMoves, Bitboard moveSq, Bitboard captSq, Bitboard pawns)
{
  while (pawns != 0)
  {
    Square ip = nextSquare(pawns);
    Bitboard endSquares = (plt::pawnMasks[cMy][ip] & moveSq)
                 | (plt::pawnCaptureMasks[cMy][ip] & captSq);

    Bitboard destSquares = 0;
    while (endSquares != 0)
    {
      Square fp = nextSquare(endSquares);
      destSquares |= 1ULL << fp;
    }
    myMoves.add(ip, destSquares);
  }
}


template <Color cMy, ShifterFunc shift>
static inline void
pawnMovement(const ChessBoard& pos, MoveList &myMoves)
{
  const Bitboard rank_3 = cMy == WHITE ? Rank3 : Rank6;
  const Bitboard rank_7 = cMy == WHITE ? Rank7 : Rank2;
  const int c = int(cMy);

  int KA = myMoves.checkers;
  Bitboard pinPieces = myMoves.pinnedPiecesSquares;
  Bitboard atkArea   = myMoves.legalSquaresMaskInCheck;

  Bitboard pawns   = pos.piece<cMy, PAWN>() & (~pinPieces);
  Bitboard ePawns = pawns & rank_7;
  pawns ^= ePawns;
  Bitboard lPawns = pawns & RightAttkingPawns;
  Bitboard rPawns = pawns &  LeftAttkingPawns;
  Bitboard sPawns = (shift(pawns, 8) & (~pos.all())) & rank_3;

  Bitboard freeSq = ~pos.all();
  Bitboard enemyP  = pos.piece< ~cMy, ALL>();
  Bitboard captSq = (atkArea &  enemyP) * (KA) + (enemyP) * (1 - KA);
  Bitboard moveSq = (atkArea ^  captSq) * (KA) + (freeSq) * (1 - KA);

  Bitboard lCaptures = shift(lPawns, 7 + 2 * c) & captSq;
  Bitboard rCaptures = shift(rPawns, 7 + 2 * (1 - c)) & captSq;

  enpassantPawns<cMy, shift>(pos, myMoves, lPawns, rPawns, KA);
  promotionPawns<cMy>(myMoves, moveSq, captSq, ePawns);

  myMoves.addPawns(0, lCaptures);
  myMoves.addPawns(1, rCaptures);

  myMoves.addPawns(2, shift(sPawns, 8) & moveSq);
  myMoves.addPawns(3, shift(pawns , 8) & moveSq);
}

#endif

template <Color cMy, BitboardFunc bitboardFunc, const MaskTable& maskTable, char pawn>
static Bitboard
pinsCheck(const ChessBoard& pos, MoveList& myMoves, Bitboard slidingPieceMy, Bitboard slidingPieceEmy)
{
  const int KA = myMoves.checkers;
  Square kpos = squareNo( pos.piece<cMy, KING>() );
  Bitboard occupied = pos.all();

  Bitboard pieces = maskTable[kpos] & occupied;
  Bitboard firstPiece  = bitboardFunc(pieces);
  Bitboard secondPiece = bitboardFunc(pieces ^ firstPiece);

  if (   !(firstPiece  & pos.piece<cMy, ALL>())
      or !(secondPiece & slidingPieceEmy)) return 0;

  Bitboard pinnedPieces = firstPiece;

  Square indexF = squareNo(firstPiece);
  Square indexS = squareNo(secondPiece);

  if (KA == 1)
    return pinnedPieces;

  if ((firstPiece & slidingPieceMy) != 0)
  {
    Bitboard destSq  = maskTable[kpos] ^ maskTable[indexS] ^ firstPiece;

    myMoves.add(indexF, destSq);
    return pinnedPieces;
  }

  constexpr Bitboard Rank63[2] = {Rank6, Rank3};

  Square eps = pos.enPassantSquare();
  const auto shift = (cMy == WHITE) ? (leftShift) : (rightShift);

  Bitboard nPawn  = firstPiece;
  Bitboard freeSq = ~pos.all();
  Bitboard sPawn  = shift(nPawn, 8) & freeSq;
  Bitboard ssPawn = shift(sPawn & Rank63[cMy], 8) & freeSq;

  if ((pawn == 's') and (firstPiece & pos.piece<cMy, PAWN>()))
    myMoves.add(indexF, sPawn | ssPawn);

  if ((pawn == 'c') and (firstPiece & pos.piece<cMy, PAWN>()))
  {
    Bitboard captSq = plt::pawnCaptureMasks[cMy][indexF];
    Bitboard destSq = captSq & secondPiece;

    if ((eps != SQUARE_NB) and (maskTable[kpos] & captSq & (1ULL << eps)) != 0)
      myMoves.enpassantPawns |= 1ULL << indexF;
    else
      myMoves.add(indexF, destSq);
  }

  return pinnedPieces;
}

template <Color cMy>
static Bitboard
pinnedPiecesList(const ChessBoard& pos, MoveList &myMoves)
{
  constexpr Color cEmy = ~cMy;
  Square kpos = squareNo( pos.piece<cMy, KING>() );

  Bitboard erq = pos.piece<cEmy, QUEEN>() | pos.piece<cEmy, ROOK  >();
  Bitboard ebq = pos.piece<cEmy, QUEEN>() | pos.piece<cEmy, BISHOP>();
  Bitboard  rq = pos.piece<cMy , QUEEN>() | pos.piece<cMy , ROOK  >();
  Bitboard  bq = pos.piece<cMy , QUEEN>() | pos.piece<cMy , BISHOP>();

  if ((  plt::lineMasks[kpos] & erq) == 0 and
    (plt::diagonalMasks[kpos] & ebq) == 0) return 0ULL;

  Bitboard pinnedPieces = 0;

  pinnedPieces |= pinsCheck<cMy, lsb, plt::rightMasks, '-'>(pos, myMoves, rq, erq);
  pinnedPieces |= pinsCheck<cMy, msb, plt::leftMasks , '-'>(pos, myMoves, rq, erq);
  pinnedPieces |= pinsCheck<cMy, lsb, plt::upMasks   , 's'>(pos, myMoves, rq, erq);
  pinnedPieces |= pinsCheck<cMy, msb, plt::downMasks , 's'>(pos, myMoves, rq, erq);

  pinnedPieces |= pinsCheck<cMy, lsb, plt::upRightMasks  , 'c'>(pos, myMoves, bq, ebq);
  pinnedPieces |= pinsCheck<cMy, lsb, plt::upLeftMasks   , 'c'>(pos, myMoves, bq, ebq);
  pinnedPieces |= pinsCheck<cMy, msb, plt::downRightMasks, 'c'>(pos, myMoves, bq, ebq);
  pinnedPieces |= pinsCheck<cMy, msb, plt::downLeftMasks , 'c'>(pos, myMoves, bq, ebq);

  return pinnedPieces;
}


template <PieceType pt>
static Bitboard
legalSquares(Square sq, Bitboard occupiedMy, Bitboard occupied)
{ return ~occupiedMy & attackSquares<pt>(sq, occupied); }


template <Color cMy, PieceType pt, PieceType... rest>
static void
addLegalSquares(const ChessBoard& pos, MoveList& myMoves)
{
  int KA = myMoves.checkers;
  Bitboard validSquares = KA * myMoves.legalSquaresMaskInCheck + (1 - KA) * AllSquares;
  Bitboard ownPieces = pos.piece<  cMy, ALL>();
  Bitboard occupied  = pos.all();
  Bitboard pieceBb   = pos.piece<cMy, pt>() & (~myMoves.pinnedPiecesSquares);

  while (pieceBb != 0)
  {
    Square ip = nextSquare(pieceBb);
    Bitboard destSquares  = legalSquares<pt>(ip, ownPieces, occupied) & validSquares;

    myMoves.add(ip, destSquares);
  }

  if constexpr (sizeof...(rest) > 0)
    addLegalSquares<cMy, rest...>(pos, myMoves);
}

template <Color cMy, ShifterFunc shift>
static void
pieceMovement(const ChessBoard& pos, MoveList& myMoves)
{
  myMoves.pinnedPiecesSquares  = pinnedPiecesList<cMy>(pos, myMoves);

  pawnMovement<cMy, shift>(pos, myMoves);
  addLegalSquares<cMy, BISHOP, KNIGHT, ROOK, QUEEN>(pos, myMoves);
}

#endif

#ifndef KING_MOVE_GENERATION

template <Color cMy, PieceType pt, PieceType... rest>
Bitboard
attackedSquaresGen(const ChessBoard& pos, Bitboard occupied)
{
  Bitboard squares = 0;
  Bitboard pieceBb = pos.piece<cMy, pt>();

  while (pieceBb != 0)
    squares |= attackSquares<pt>(nextSquare(pieceBb), occupied);

  if constexpr (sizeof...(rest) > 0)
    squares |= attackedSquaresGen<cMy, rest...>(pos, occupied);

  return squares;
}

template <Color side>
static Bitboard
generateAttackedSquares(const ChessBoard& pos, Bitboard occupied)
{
  return pawnAttackSquares<side>(pos)
      | attackedSquaresGen<side, BISHOP, KNIGHT, ROOK, QUEEN, KING>(pos, occupied);
}


template <Color cMy, PieceType pt, PieceType emy>
static void
addAttacker(const ChessBoard& pos, int& attackerCount, Bitboard& attackedMask)
{
  Square kSq = squareNo(pos.piece<cMy, KING>());
  Bitboard occupied = pos.all();
  Bitboard enemyBb = pos.piece<~cMy, pt>() | pos.piece<~cMy, emy>();

  Bitboard kingMask  = attackSquares<pt>(kSq, occupied);
  Bitboard pieceMask = kingMask & enemyBb;

  if (pieceMask == 0)
    return;

  if ((pieceMask & (pieceMask - 1)) != 0)
    ++attackerCount;

  ++attackerCount;
  attackedMask |= (kingMask & attackSquares<pt>(squareNo(pieceMask), occupied)) | pieceMask;
}


template <Color cMy>
static void
kingAttackers(const ChessBoard& pos, MoveList& myMoves)
{
  Bitboard attackedSq = myMoves.enemyAttackedSquares;
  constexpr Color cEmy = ~cMy;

  if (attackedSq && ((pos.piece<cMy, KING>() & attackedSq)) == 0)
    return;

  Square kpos = squareNo(pos.piece<cMy, KING>());

  int attackerCount = 0;
  Bitboard attackedMask = 0;

  addAttacker<cMy, ROOK  , QUEEN>(pos, attackerCount, attackedMask);
  addAttacker<cMy, BISHOP, QUEEN>(pos, attackerCount, attackedMask);
  addAttacker<cMy, KNIGHT, NONE >(pos, attackerCount, attackedMask);

  Bitboard pawnSq = plt::pawnCaptureMasks[cMy][kpos] & pos.piece<cEmy, PAWN>();
  if (pawnSq != 0)
  {
    ++attackerCount;
    attackedMask |= pawnSq;
  }

  myMoves.checkers = attackerCount;
  myMoves.legalSquaresMaskInCheck = attackedMask;
}

template <Color cMy>
static void
kingMoves(const ChessBoard& pos, MoveList& myMoves)
{
  constexpr Color cEmy = ~cMy;
  Square kpos = squareNo(pos.piece<cMy, KING>());

  Bitboard attackedSq = myMoves.enemyAttackedSquares;
  Bitboard kingMask = plt::kingMasks[kpos];
  Bitboard destSq  = kingMask & (~(pos.piece<cMy, ALL>() | attackedSq));

  myMoves.add(kpos, destSq);

  if (!(pos.csep & 1920) or ((1ULL << kpos) & attackedSq)) {
    // no castling move available, thus early exit
    return;
  }

  Bitboard apieces = pos.all();
  Bitboard coveredSquares = apieces | attackedSq;

  constexpr int      shift  = 56 * cEmy;
  constexpr Bitboard lMidSq = 2ULL << shift;
  constexpr Bitboard rSq    = 96ULL << shift;
  constexpr Bitboard lSq    = 12ULL << shift;

  constexpr int kingSide  = 256 << (2 * cMy);
  constexpr int queenSide = 128 << (2 * cMy);

  // Can castle king_side  and no pieces are in-between
  if ((pos.csep & kingSide) and !(rSq & coveredSquares))
    destSq |= 1ULL << (shift + 6);

  // Can castle queen_side and no pieces are in-between
  if ((pos.csep & queenSide) and !(apieces & lMidSq) and !(lSq & coveredSquares))
    destSq |= 1ULL << (shift + 2);

  myMoves.add(kpos, destSq);
}


#endif


#ifndef GENERATOR


/**
 * @brief Calculates the bitboard of initial squares from which a moving piece
 * could potentially give a discovered check to the opponent's king.
**/
template <Color cMy>
static Bitboard
squaresForDiscoveredCheck(const ChessBoard& pos, MoveList& myMoves)
{
  // Implementation to calculate and return the bitboard of squares.
  // These squares are the initial positions from which a piece could move
  // to potentially give a discovered check to the enemy king.

  constexpr Color cEmy = ~cMy;

  Square kSq = squareNo( pos.piece<cEmy, KING>() );

  Bitboard rq = pos.piece<cMy, QUEEN>() | pos.piece<cMy, ROOK  >();
  Bitboard bq = pos.piece<cMy, QUEEN>() | pos.piece<cMy, BISHOP>();

  Bitboard occupied = pos.all();
  Bitboard occupiedMy = pos.piece<cMy, ALL>();
  Bitboard calculatedBitboard = 0;

  const auto checkSquare = [&] (const auto& __f, const MaskTable& table, Bitboard myPiece)
  {
    Bitboard mask = table[kSq] & occupied;
    Bitboard firstPiece  = __f(mask);
    Bitboard secondPiece = __f(mask ^ firstPiece);

    if ((firstPiece & occupiedMy) and (secondPiece & myPiece))
    {
      myMoves.discoverCheckMasks[squareNo(firstPiece)] = table[kSq] & ~table[squareNo(secondPiece)];
      calculatedBitboard |= firstPiece;
    }
  };

  if (rq)
  {
    checkSquare(lsb, plt::rightMasks, rq);
    checkSquare(msb, plt::leftMasks , rq);
    checkSquare(lsb, plt::upMasks   , rq);
    checkSquare(msb, plt::downMasks , rq);
  }

  if (bq)
  {
    checkSquare(lsb, plt::upRightMasks  , bq);
    checkSquare(lsb, plt::upLeftMasks   , bq);
    checkSquare(msb, plt::downRightMasks, bq);
    checkSquare(msb, plt::downLeftMasks , bq);
  }

  return calculatedBitboard;
}

template <Color cMy>
static void
generateSquaresThatCheckEnemyKing(const ChessBoard& pos, MoveList& myMoves)
{
  constexpr Color cEmy = ~cMy;

  Square kSq = squareNo(pos.piece<cEmy, KING>());
  Bitboard occupied = pos.all();

  myMoves.squaresThatCheckEnemyKing[0] = plt::pawnCaptureMasks[cEmy][kSq];
  myMoves.squaresThatCheckEnemyKing[1] = attackSquares<BISHOP>(kSq, occupied);
  myMoves.squaresThatCheckEnemyKing[2] = attackSquares<KNIGHT>(kSq, occupied);
  myMoves.squaresThatCheckEnemyKing[3] = attackSquares< ROOK >(kSq, occupied);
  myMoves.squaresThatCheckEnemyKing[4] =
    myMoves.squaresThatCheckEnemyKing[1] | myMoves.squaresThatCheckEnemyKing[3];
}

template <MoveGenStage stage, Color cMy>
static inline void
stagedGenerateMovesImpl(const ChessBoard& pos, MoveList& myMoves)
{
  if constexpr (stage == GEN_METADATA)
  {
    myMoves.color = cMy;
    myMoves.myPawns = pos.piece<cMy, PAWN>();
    myMoves.myAttackedSquares = generateAttackedSquares<cMy>(pos, pos.all());
    myMoves.enemyAttackedSquares = generateAttackedSquares<~cMy>(pos, pos.all() ^ pos.piece<cMy, KING>());

    kingAttackers<cMy>(pos, myMoves);
  }
  else if constexpr (stage == GEN_MOVES)
  {
    constexpr ShifterFunc shift = (cMy == WHITE) ? leftShift : rightShift;

    if (myMoves.checkers < 2)
      pieceMovement<cMy, shift>(pos, myMoves);

    kingMoves<cMy>(pos, myMoves);
  }
  else if constexpr (stage == GEN_CHECKS)
  {
    myMoves.discoverCheckSquares = squaresForDiscoveredCheck<cMy>(pos, myMoves);
    generateSquaresThatCheckEnemyKing<cMy>(pos, myMoves);
  }
}


template <MoveGenStage stage>
void
stagedGenerateMoves(const ChessBoard& pos, MoveList& myMoves)
{
  pos.color == WHITE
    ? stagedGenerateMovesImpl<stage, WHITE>(pos, myMoves)
    : stagedGenerateMovesImpl<stage, BLACK>(pos, myMoves);
}

template void stagedGenerateMoves<GEN_METADATA>(const ChessBoard&, MoveList&);
template void stagedGenerateMoves<GEN_MOVES   >(const ChessBoard&, MoveList&);
template void stagedGenerateMoves<GEN_CHECKS  >(const ChessBoard&, MoveList&);


MoveList
generateMoves(const ChessBoard& pos, bool generateChecksData)
{
  MoveList myMoves;

  stagedGenerateMoves<GEN_METADATA>(pos, myMoves);
  stagedGenerateMoves<GEN_MOVES>(pos, myMoves);

  if (generateChecksData)
    stagedGenerateMoves<GEN_CHECKS>(pos, myMoves);

  return myMoves;
}


template<Color cMy, PieceType pt, PieceType... rest>
Square
isTrapped(const ChessBoard& pos, Bitboard enemyAttackedSquares)
{
  Bitboard piece = pos.piece<cMy, pt>();
  while (piece)
  {
    Square pieceSq = nextSquare(piece);
    Bitboard  mask = legalSquares<pt>(pieceSq, pos.piece<cMy, ALL>(), pos.all());

    if (((1ULL << pieceSq) & enemyAttackedSquares) and (mask & enemyAttackedSquares) == mask and !(mask & (1ULL << pieceSq)))
      return pieceSq;
  }

  if constexpr (sizeof...(rest) > 0)
    return isTrapped<cMy, rest...>(pos, enemyAttackedSquares);

  return SQUARE_NB;
}

bool
pieceTrapped(const ChessBoard& pos, Bitboard myAttackedBB, Bitboard enemyAttackedBB)
{
  const Square square = pos.color == WHITE
    ? isTrapped<WHITE, QUEEN, ROOK, BISHOP, KNIGHT>(pos, enemyAttackedBB)
    : isTrapped<BLACK, QUEEN, ROOK, BISHOP, KNIGHT>(pos, enemyAttackedBB);

  if (square != SQUARE_NB)
  {
    Bitboard myAttackedSquares = myAttackedBB;

    Weight maxValueCapture = 0;

    for (PieceType pt = PAWN; pt < KING; pt = PieceType(pt + 1))
      if (pos.getPiece(~pos.color, pt) & myAttackedSquares)
        maxValueCapture = pos.pieceValue(pt);

    Weight trappedPieceValue = pos.pieceValue(type_of(pos.pieceOnSquare(square)));

    if (maxValueCapture >= trappedPieceValue)
      return false;

    if ((1ULL << square) & myAttackedSquares)
    {
      Square  eptSq = getSmallestAttacker(pos, square, ~pos.color, 0);
      PieceType ept = type_of(pos.pieceOnSquare(eptSq));
      if (((1ULL << eptSq) & myAttackedSquares) or (pos.pieceValue(ept) + PawnValueMg >= trappedPieceValue))
        return false;
    }

    return true;
  }

  return false;
}

template<Color side, PieceType piece, PieceType... rest>
Square
smallestAttacker(const ChessBoard& pos, const Square sq, Bitboard removedPieces)
{
  Bitboard pieceBb = pos.piece<side, piece>();
  Bitboard mask;

  if constexpr (piece == PAWN)
    mask = attackSquares<~side, PAWN>(sq);
  else
    mask = attackSquares<piece>(sq, pos.all() ^ removedPieces);

  mask &= pieceBb ^ (pieceBb & removedPieces);
  if (mask) return msbIndex(mask);

  if constexpr (sizeof...(rest) > 0)
    return smallestAttacker<side, rest...>(pos, sq, removedPieces);

  return SQUARE_NB;
}

template <Color side>
Square getSmallestAttackerImpl(const ChessBoard& pos, const Square square, Bitboard removedPieces) {
  return smallestAttacker<side, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING>(pos, square, removedPieces);
}

Square
getSmallestAttacker(const ChessBoard& pos, const Square square, Color side, Bitboard removedPieces)
{
  return side == WHITE
    ? getSmallestAttackerImpl<WHITE>(pos, square, removedPieces)
    : getSmallestAttackerImpl<BLACK>(pos, square, removedPieces);
}

#endif


#ifndef HASH_MOVE_LEGAL

// ----- King-ray dispatch table used by pinnedRayDest_fast ---------------------
// Each square is on at most one of the eight king-aligned rays. rayDirection()
// computes which one (or DIR_NONE) in pure arithmetic, then we look the ray
// up in RAY_DISPATCH instead of trying all eight in sequence.

namespace {

constexpr int DIR_NONE = 8;

// 15x15 lookup keyed on (dr+7, df+7), dr/df in [-7, 7]. One IMUL, one ADD,
// one byte load -- no branches. Built once at program load via constexpr.
//   0 right  1 left   2 up        3 down
//   4 upRt   5 upLft  6 dnRt      7 dnLft   8 not on any ray
constexpr auto RAY_DIR_LUT = []() {
  std::array<uint8_t, 15 * 15> t{};
  for (auto& v : t) v = uint8_t(DIR_NONE);
  for (int dr = -7; dr <= 7; ++dr)
    for (int df = -7; df <= 7; ++df)
    {
      int code = DIR_NONE;
      if (dr == 0 && df == 0)             code = DIR_NONE;
      else if (dr == 0)                   code = df > 0 ? 0 : 1;
      else if (df == 0)                   code = dr > 0 ? 2 : 3;
      else if (dr ==  df)                 code = dr > 0 ? 4 : 7;
      else if (dr == -df)                 code = dr > 0 ? 5 : 6;
      t[size_t((dr + 7) * 15 + (df + 7))] = uint8_t(code);
    }
  return t;
}();

static inline int
rayDirection(Square from, Square to) noexcept
{
  const int dr = (int(to) >> 3) - (int(from) >> 3);
  const int df = (int(to) &  7) - (int(from) &  7);
  return RAY_DIR_LUT[size_t((dr + 7) * 15 + (df + 7))];
}

struct RayDispatch
{
  const MaskTable* table;
  BitboardFunc     closestToKing;   // lsb for rays toward higher squares, msb otherwise
  bool             diagonal;        // false: pinner must be R/Q; true: B/Q
};

static constexpr RayDispatch RAY_DISPATCH[8] = {
  { &plt::rightMasks    , lsb, false },           // 0 right
  { &plt::leftMasks     , msb, false },           // 1 left
  { &plt::upMasks       , lsb, false },           // 2 up
  { &plt::downMasks     , msb, false },           // 3 down
  { &plt::upRightMasks  , lsb, true  },           // 4 upRight
  { &plt::upLeftMasks   , lsb, true  },           // 5 upLeft
  { &plt::downRightMasks, msb, true  },           // 6 downRight
  { &plt::downLeftMasks , msb, true  },           // 7 downLeft
};

} // namespace

// Like pinnedRayDest below, but uses rayDirection(kpos, ip) to jump straight
// to the one ray that could contain `ip`, avoiding up to seven masked-load +
// AND tests on the common "ip not on any king ray" path.
template <Color cMy>
static bool
pinnedRayDest_fast(const ChessBoard& pos, Square ip, Square kpos, Bitboard& destSq) noexcept
{
  const int dir = rayDirection(kpos, ip);
  if (dir == DIR_NONE) return false;

  constexpr Color cEmy = ~cMy;
  const RayDispatch& d = RAY_DISPATCH[dir];
  const Bitboard sliders = d.diagonal
    ? (pos.piece<cEmy, QUEEN>() | pos.piece<cEmy, BISHOP>())
    : (pos.piece<cEmy, QUEEN>() | pos.piece<cEmy, ROOK  >());

  const MaskTable& table = *d.table;
  const Bitboard ray   = table[kpos];
  const Bitboard ipBit = 1ULL << ip;
  const Bitboard pieces = ray & pos.all();

  const Bitboard first = d.closestToKing(pieces);
  if (first != ipBit) return false;               // a blocker sits between king and ip

  const Bitboard second = d.closestToKing(pieces ^ first);
  if (!(second & sliders)) return false;          // no pinner behind ip

  destSq = ray ^ table[squareNo(second)] ^ ipBit;
  return true;
}

// If the piece on `ip` is pinned against our king, fills `destSq` with the
// squares it may legally move to (the squares between king and pinner, plus the
// pinner's square, minus `ip`) and returns true. Otherwise leaves `destSq`
// untouched and returns false. Only the single ray that could contain `ip` is
// ever examined.
//
// Superseded by pinnedRayDest_fast (which uses rayDirection() to skip the
// per-direction `ray & ipBit` probes). Kept here as a reference / fallback.
template <Color cMy>
[[maybe_unused]] static bool
pinnedRayDest(const ChessBoard& pos, Square ip, Square kpos, Bitboard& destSq) noexcept
{
  constexpr Color cEmy = ~cMy;
  const Bitboard ipBit = 1ULL << ip;
  const Bitboard occupied = pos.all();
  const Bitboard erq = pos.piece<cEmy, QUEEN>() | pos.piece<cEmy, ROOK  >();
  const Bitboard ebq = pos.piece<cEmy, QUEEN>() | pos.piece<cEmy, BISHOP>();

  // -1 -> ip not on this ray; 0 -> on ray but not pinned; 1 -> pinned (destSq set)
  const auto scan = [&] (const MaskTable& table, BitboardFunc closest, Bitboard sliders) -> int
  {
    const Bitboard ray = table[kpos];
    if (!(ray & ipBit)) return -1;

    const Bitboard pieces = ray & occupied;
    const Bitboard first  = closest(pieces);
    if (first != ipBit) return 0;                 // a blocker sits between king and ip

    const Bitboard second = closest(pieces ^ first);
    if (!(second & sliders)) return 0;            // nothing pinning behind ip

    destSq = ray ^ table[squareNo(second)] ^ ipBit;
    return 1;
  };

  int r;
  if ((r = scan(plt::rightMasks    , lsb, erq)) != -1) return r == 1;
  if ((r = scan(plt::leftMasks     , msb, erq)) != -1) return r == 1;
  if ((r = scan(plt::upMasks       , lsb, erq)) != -1) return r == 1;
  if ((r = scan(plt::downMasks     , msb, erq)) != -1) return r == 1;
  if ((r = scan(plt::upRightMasks  , lsb, ebq)) != -1) return r == 1;
  if ((r = scan(plt::upLeftMasks   , lsb, ebq)) != -1) return r == 1;
  if ((r = scan(plt::downRightMasks, msb, ebq)) != -1) return r == 1;
  if ((r = scan(plt::downLeftMasks , msb, ebq)) != -1) return r == 1;
  return false;
}

// ----- micro-benchmark: pinnedRayDest vs pinnedRayDest_fast --------------------
// Runs each function `iters` times over every non-king own-side piece in `pos`
// (`pos.color`) and returns the total wall time in seconds. The accumulator
// (`sink`) is XORed into a volatile to keep the compiler from folding the loops
// away. Both variants get the same input set and are timed back-to-back; for a
// single position the absolute numbers are noisy, so the caller should sum them
// over many positions before drawing conclusions.
template <Color cMy>
static std::pair<double, double>
benchmarkPinnedRayDestImpl(const ChessBoard& pos, int iters) noexcept
{
  const Square kpos = squareNo(pos.piece<cMy, KING>());

  Square pieces[16];
  int    pieceCount = 0;
  Bitboard own = pos.piece<cMy, PAWN>()  | pos.piece<cMy, BISHOP>()
               | pos.piece<cMy, KNIGHT>()| pos.piece<cMy, ROOK>()
               | pos.piece<cMy, QUEEN>();
  while (own && pieceCount < 16)
  {
    const Bitboard b = own & -own;
    pieces[pieceCount++] = squareNo(b);
    own ^= b;
  }

  volatile Bitboard volSink = 0;
  Bitboard sinkSlow = 0, sinkFast = 0;

  using clock = std::chrono::high_resolution_clock;

  const auto t0 = clock::now();
  for (int it = 0; it < iters; ++it)
    for (int i = 0; i < pieceCount; ++i)
    {
      Bitboard dst = AllSquares;
      pinnedRayDest<cMy>(pos, pieces[i], kpos, dst);
      sinkSlow ^= dst;
    }
  const auto t1 = clock::now();
  for (int it = 0; it < iters; ++it)
    for (int i = 0; i < pieceCount; ++i)
    {
      Bitboard dst = AllSquares;
      pinnedRayDest_fast<cMy>(pos, pieces[i], kpos, dst);
      sinkFast ^= dst;
    }
  const auto t2 = clock::now();

  volSink = sinkSlow ^ sinkFast;
  (void) volSink;

  return {
    std::chrono::duration<double>(t1 - t0).count(),
    std::chrono::duration<double>(t2 - t1).count()
  };
}

std::pair<double, double>
benchmarkPinnedRayDest(const ChessBoard& pos, int iters)
{
  return (pos.color == WHITE)
    ? benchmarkPinnedRayDestImpl<WHITE>(pos, iters)
    : benchmarkPinnedRayDestImpl<BLACK>(pos, iters);
}

template <Color cMy>
static bool
isHashMoveLegalImpl(Move move, const ChessBoard& pos, const MoveList& meta) noexcept
{
  constexpr Color    cEmy      = ~cMy;
  constexpr int      pawnPush  = (cMy == WHITE) ? 8 : -8;
  constexpr Bitboard startRank = (cMy == WHITE) ? Rank2 : Rank7;

  const Square ip = from_sq(move);
  const Square fp = to_sq(move);
  if (ip == fp) return false;

  const Piece ipPiece = pos.pieceOnSquare(ip);
  if (ipPiece == NO_PIECE || color_of(ipPiece) != cMy) return false;

  const PieceType ipt = type_of(ipPiece);
  if (PieceType((move >> 12) & 7) != ipt) return false;                 // pIp field

  const Piece fpPiece = pos.pieceOnSquare(fp);
  if (fpPiece != NO_PIECE && color_of(fpPiece) == cMy) return false;    // can't capture own
  if (PieceType((move >> 15) & 7) != type_of(fpPiece)) return false;    // pFp field (NONE on empty / e.p. target)

  const Bitboard ipBit = 1ULL << ip;
  const Bitboard fpBit = 1ULL << fp;
  const Bitboard occupied = pos.all();
  const Square kpos = squareNo(pos.piece<cMy, KING>());

  // --------------------------------------------------------------------- King
  if (ipt == KING)
  {
    const int diff = int(fp) - int(ip);

    if (diff == 2 || diff == -2)                                        // castling
    {
      if (meta.checkers != 0) return false;                            // never out of check
      const Bitboard attacked = meta.enemyAttackedSquares;
      if (ipBit & attacked) return false;                              // (king-square attacked => in check)

      constexpr int shift = 56 * int(cEmy);
      if (int(ip) != shift + 4) return false;                          // king must be on its home square

      const Bitboard covered = occupied | attacked;
      if (diff == 2)                                                   // king side
      {
        constexpr int      kingSide = 256 << (2 * int(cMy));
        constexpr Bitboard rSq      = 96ULL << shift;                  // f1/g1 (or f8/g8)
        if (!(pos.csep & kingSide)) return false;
        if (rSq & covered) return false;
        return true;
      }
      // queen side
      constexpr int      queenSide = 128 << (2 * int(cMy));
      constexpr Bitboard lMidSq    = 2ULL  << shift;                   // b1 (or b8)
      constexpr Bitboard lSq       = 12ULL << shift;                   // c1/d1 (or c8/d8)
      if (!(pos.csep & queenSide)) return false;
      if (occupied & lMidSq) return false;
      if (lSq & covered) return false;
      return true;
    }

    // ordinary king step
    if (!(plt::kingMasks[ip] & fpBit)) return false;
    if (fpBit & meta.enemyAttackedSquares) return false;
    return true;
  }

  // any non-king move is impossible under double check
  if (meta.checkers >= 2) return false;

  // pin ray + (when in check) the squares that block / capture the checker
  Bitboard pinMask = AllSquares;
  // Old: pinnedRayDest<cMy>(pos, ip, kpos, pinMask) — 8-direction scan, kept above for reference.
  const bool pinned = pinnedRayDest_fast<cMy>(pos, ip, kpos, pinMask);  // pinMask unchanged if not pinned
  const Bitboard checkMask = (meta.checkers == 1) ? meta.legalSquaresMaskInCheck : AllSquares;

  // --------------------------------------------------------------------- Pawn
  if (ipt == PAWN)
  {
    const Square ep = pos.enPassantSquare();

    if (fpBit & plt::pawnCaptureMasks[cMy][ip])                        // diagonal -> capture or e.p.
    {
      if (ep != SQUARE_NB && fp == ep)                                // en passant
      {
        const Square capSq = fp - pawnPush;                           // the pawn we remove
        if (pos.pieceOnSquare(capSq) != make_piece(cEmy, PAWN)) return false;
        if (meta.checkers == 1
          && !(plt::pawnCaptureMasks[cMy][kpos] & pos.piece<cEmy, PAWN>())) return false;
        if (!(fpBit & pinMask)) return false;
        // When the pawn is pinned along its capture diagonal, the pin already
        // guarantees safety w.r.t. that slider — generation's pinsCheck branch
        // skips enpassantRecheck for exactly this reason; mirror it here.
        if (!pinned && !enpassantRecheck<cMy>(ip, pos)) return false;
        return true;
      }
      if (fpPiece == NO_PIECE) return false;                          // nothing there to capture
      // (enemy colour and pFp already validated above)
    }
    else                                                              // straight push
    {
      if (fpPiece != NO_PIECE) return false;
      if (int(fp) == int(ip) + pawnPush)
        ;                                                             // single push
      else if (int(fp) == int(ip) + 2 * pawnPush && (ipBit & startRank))
      {
        if (pos.pieceOnSquare(Square(int(ip) + pawnPush)) != NO_PIECE) return false;
      }
      else return false;
    }

    if (!(fpBit & checkMask)) return false;
    if (!(fpBit & pinMask)) return false;
    return true;
  }

  // ------------------------------------------------------------------- Knight
  if (ipt == KNIGHT)
  {
    if (!(plt::knightMasks[ip] & fpBit)) return false;
    if (!(fpBit & checkMask)) return false;
    if (!(fpBit & pinMask)) return false;                             // a pinned knight can never move
    return true;
  }

  // ----------------------------------------------------- Bishop / Rook / Queen
  Bitboard reachable;
  if      (ipt == BISHOP) reachable = attackSquares<BISHOP>(ip, occupied);
  else if (ipt == ROOK  ) reachable = attackSquares<ROOK  >(ip, occupied);
  else                    reachable = attackSquares<QUEEN >(ip, occupied);

  if (!(reachable & fpBit)) return false;
  if (!(fpBit & checkMask)) return false;
  if (!(fpBit & pinMask)) return false;
  return true;
}

bool
isHashMoveLegal(Move move, const ChessBoard& pos, const MoveList& meta)
{
  return pos.color == WHITE
    ? isHashMoveLegalImpl<WHITE>(move, pos, meta)
    : isHashMoveLegalImpl<BLACK>(move, pos, meta);
}

#endif

