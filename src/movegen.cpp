
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

  if ((type_of(ipt) != PAWN))
    return ((1ULL << fp) & myMoves.destSquares[ip]) != 0;

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

  if (!(pos.csep & 1920) or ((1ULL << kpos) & attackedSq)) return;

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

template <Color cMy, ShifterFunc shift>
static inline MoveList
moveGenerator(const ChessBoard& pos, bool generateChecksData)
{
  MoveList myMoves(cMy);

  myMoves.myPawns = pos.piece<cMy, PAWN>();
  myMoves.myAttackedSquares = generateAttackedSquares<cMy>(pos, pos.all());
  myMoves.enemyAttackedSquares = generateAttackedSquares<~cMy>(pos, pos.all() ^ pos.piece<cMy, KING>());

  kingAttackers<cMy>(pos, myMoves);

  if (myMoves.checkers < 2)
    pieceMovement<cMy, shift>(pos, myMoves);

  kingMoves<cMy>(pos, myMoves);

  if (generateChecksData)
  {
    myMoves.discoverCheckSquares = squaresForDiscoveredCheck<cMy>(pos, myMoves);
    generateSquaresThatCheckEnemyKing<cMy>(pos, myMoves);
  }

  return myMoves;
}


MoveList
generateMoves(const ChessBoard& pos, bool generateChecksData)
{
  return pos.color == WHITE ?
    moveGenerator<WHITE,  leftShift>(pos, generateChecksData)
  : moveGenerator<BLACK, rightShift>(pos, generateChecksData);
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

