
#include "attacks.h"
#include "base_utils.h"
#include "movegen.h"


#ifndef MOVEGEN_UTILS

bool
IsLegalMoveForPosition(Move move, ChessBoard& pos)
{
  // TODO: Add pseudo checks and test it
  Square ip = Square(move & 63);
  Square fp = Square((move >> 6) & 63);

  Piece ipt = pos.PieceOnSquare(ip);
  Piece fpt = pos.PieceOnSquare(fp);

  Color side = pos.color;

  // If no piece on init_sq or piece_color != side_to_move
  if (((ipt & 7) == 0) or (((ipt >> 3) & 1) != side))
    return false;

  // If capt_piece_color == side_to_move
  if ((fpt != 0) and ((fpt >> 3) & 1) == side)
    return false;

  MoveList myMoves = GenerateMoves(pos);
  MoveArray movesArray;
  myMoves.getMoves(pos, movesArray);

  for (const Move legal_move : movesArray)
    if (filter(move) == filter(legal_move)) return true;

  return false;
}


#endif

#ifndef Piece_Movement

#ifndef PAWNS

template <Color c_my>
static bool
EnpassantRecheck(Square ip, const ChessBoard& _cb)
{
  constexpr Color c_emy = ~c_my;

  Square kpos = SquareNo( _cb.piece<c_my, KING>() );
  Square eps = _cb.EnPassantSquare();

  Bitboard erq = _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, ROOK>();
  Bitboard ebq = _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, BISHOP>();
  Bitboard  Ap = _cb.All() ^ ((1ULL << ip) | (1ULL << (eps - 8 * (2 * c_my - 1))));

  Bitboard res  = Msb((plt::LeftMasks[kpos] & Ap)) | Lsb((plt::RightMasks[kpos] & Ap));
  Bitboard tmp1 = Msb((plt::DownLeftMasks[kpos] & Ap)) | Lsb((plt::UpRightMasks[kpos] & Ap));
  Bitboard tmp2 = Lsb((plt::UpLeftMasks[kpos] & Ap)) | Msb((plt::DownRightMasks[kpos] & Ap));

  if (res & erq) return false;
  // Board has to be invalid for this check
  if ((tmp1 | tmp2) & ebq) return false;

  return true;
}

template <Color c_my, ShifterFunc shift>
static inline void
EnpassantPawns(const ChessBoard &_cb, MoveList &myMoves,
  Bitboard l_pawns, Bitboard r_pawns, int KA)
{
  constexpr Color c_emy = ~c_my;
  constexpr int own =  c_my << 3;
  constexpr int emy = c_emy << 3;

  Square kpos = SquareNo( _cb.piece<c_my, KING>() );
  Square eps = _cb.EnPassantSquare();
  Bitboard _ep = 1ULL << eps;

  if (eps == SQUARE_NB || (KA == 1 and !(plt::PawnCaptureMasks[c_my][kpos] & _cb.piece<c_emy, PAWN>())))
    return;

  if ((shift(l_pawns, 7 + (own >> 2)) & _ep) and EnpassantRecheck<c_my>(eps + (2 * emy - 9), _cb))
    myMoves.enpassantPawns |= 1ULL << (eps + (2 * emy - 9));

  if ((shift(r_pawns, 7 + (emy >> 2)) & _ep) and EnpassantRecheck<c_my>(eps + (2 * emy - 7), _cb))
    myMoves.enpassantPawns |= 1ULL << (eps + (2 * emy - 7));
}

template <Color c_my>
static inline void
PromotionPawns(MoveList &myMoves, Bitboard move_sq, Bitboard capt_sq, Bitboard pawns)
{
  while (pawns != 0)
  {
    Square ip = NextSquare(pawns);
    Bitboard endSquares = (plt::PawnMasks[c_my][ip] & move_sq)
                 | (plt::PawnCaptureMasks[c_my][ip] & capt_sq);

    Bitboard destSquares = 0;
    while (endSquares != 0)
    {
      Square fp = NextSquare(endSquares);
      destSquares |= 1ULL << fp;
    }
    myMoves.Add(ip, destSquares);
  }
}


template <Color c_my, ShifterFunc shift>
static inline void
PawnMovement(const ChessBoard &_cb, MoveList &myMoves,
  Bitboard pin_pieces, int KA, Bitboard atk_area)
{
  const Bitboard rank_3 = c_my == WHITE ? Rank3 : Rank6;
  const Bitboard rank_7 = c_my == WHITE ? Rank7 : Rank2;
  const int c = int(c_my);

  Bitboard pawns   = _cb.piece<c_my, PAWN>() & (~pin_pieces);
  Bitboard e_pawns = pawns & rank_7;
  pawns ^= e_pawns;
  Bitboard l_pawns = pawns & RightAttkingPawns;
  Bitboard r_pawns = pawns &  LeftAttkingPawns;
  Bitboard s_pawns = (shift(pawns, 8) & (~_cb.All())) & rank_3;

  Bitboard free_sq = ~_cb.All();
  Bitboard enemyP  = _cb.piece< ~c_my, ALL>();
  Bitboard capt_sq = (atk_area &  enemyP) * (KA) + ( enemyP) * (1 - KA);
  Bitboard move_sq = (atk_area ^ capt_sq) * (KA) + (free_sq) * (1 - KA);

  Bitboard l_captures = shift(l_pawns, 7 + 2 * c) & capt_sq;
  Bitboard r_captures = shift(r_pawns, 7 + 2 * (1 - c)) & capt_sq;

  EnpassantPawns<c_my, shift>(_cb, myMoves, l_pawns, r_pawns, KA);
  PromotionPawns<c_my>(myMoves, move_sq, capt_sq, e_pawns);

  myMoves.AddPawns(0, l_captures);
  myMoves.AddPawns(1, r_captures);

  myMoves.AddPawns(2, shift(s_pawns, 8) & move_sq);
  myMoves.AddPawns(3, shift(pawns  , 8) & move_sq);
}

#endif

template <Color c_my, BitboardFunc bitboardFunc, const MaskTable& maskTable, char pawn>
static Bitboard
PinsCheck(const ChessBoard& pos, MoveList& myMoves, int KA, Bitboard sliding_piece_my, Bitboard sliding_piece_emy)
{
  Square kpos = SquareNo( pos.piece<c_my, KING>() );
  Bitboard occupied = pos.All();

  Bitboard pieces = maskTable[kpos] & occupied;
  Bitboard first_piece  = bitboardFunc(pieces);
  Bitboard second_piece = bitboardFunc(pieces ^ first_piece);

  if (   !(first_piece  & pos.piece<c_my, ALL>())
      or !(second_piece & sliding_piece_emy)) return 0;

  Bitboard pinned_pieces = first_piece;

  Square index_f = SquareNo(first_piece);
  Square index_s = SquareNo(second_piece);

  if (KA == 1)
    return pinned_pieces;

  if ((first_piece & sliding_piece_my) != 0)
  {
    Bitboard dest_sq  = maskTable[kpos] ^ maskTable[index_s] ^ first_piece;

    myMoves.Add(index_f, dest_sq);
    return pinned_pieces;
  }

  constexpr Bitboard Rank63[2] = {Rank6, Rank3};

  Square eps = pos.EnPassantSquare();
  const auto shift = (c_my == WHITE) ? (LeftShift) : (RightShift);

  Bitboard n_pawn  = first_piece;
  Bitboard free_sq = ~pos.All();
  Bitboard s_pawn  = shift(n_pawn, 8) & free_sq;
  Bitboard ss_pawn = shift(s_pawn & Rank63[c_my], 8) & free_sq;

  if ((pawn == 's') and (first_piece & pos.piece<c_my, PAWN>()))
    myMoves.Add(index_f, s_pawn | ss_pawn);

  if ((pawn == 'c') and (first_piece & pos.piece<c_my, PAWN>()))
  {
    Bitboard capt_sq = plt::PawnCaptureMasks[c_my][index_f];
    Bitboard dest_sq = capt_sq & second_piece;

    if ((eps != 64) and (maskTable[kpos] & capt_sq & (1ULL << eps)) != 0)
      dest_sq |= 1ULL << eps;

    myMoves.Add(index_f, dest_sq);
  }

  return pinned_pieces;
}

template <Color c_my>
static Bitboard
PinnedPiecesList(const ChessBoard& pos, MoveList &myMoves, int KA)
{
  constexpr Color c_emy = ~c_my;
  Square kpos = SquareNo( pos.piece<c_my, KING>() );

  Bitboard erq = pos.piece<c_emy, QUEEN>() | pos.piece<c_emy, ROOK  >();
  Bitboard ebq = pos.piece<c_emy, QUEEN>() | pos.piece<c_emy, BISHOP>();
  Bitboard  rq = pos.piece<c_my , QUEEN>() | pos.piece<c_my , ROOK  >();
  Bitboard  bq = pos.piece<c_my , QUEEN>() | pos.piece<c_my , BISHOP>();

  if ((  plt::LineMasks[kpos] & erq) == 0 and
    (plt::DiagonalMasks[kpos] & ebq) == 0) return 0ULL;

  Bitboard pinned_pieces = 0;

  pinned_pieces |= PinsCheck<c_my, Lsb, plt::RightMasks, '-'>(pos, myMoves, KA, rq, erq);
  pinned_pieces |= PinsCheck<c_my, Msb, plt::LeftMasks , '-'>(pos, myMoves, KA, rq, erq);
  pinned_pieces |= PinsCheck<c_my, Lsb, plt::UpMasks   , 's'>(pos, myMoves, KA, rq, erq);
  pinned_pieces |= PinsCheck<c_my, Msb, plt::DownMasks , 's'>(pos, myMoves, KA, rq, erq);

  pinned_pieces |= PinsCheck<c_my, Lsb, plt::UpRightMasks  , 'c'>(pos, myMoves, KA, bq, ebq);
  pinned_pieces |= PinsCheck<c_my, Lsb, plt::UpLeftMasks   , 'c'>(pos, myMoves, KA, bq, ebq);
  pinned_pieces |= PinsCheck<c_my, Msb, plt::DownRightMasks, 'c'>(pos, myMoves, KA, bq, ebq);
  pinned_pieces |= PinsCheck<c_my, Msb, plt::DownLeftMasks , 'c'>(pos, myMoves, KA, bq, ebq);

  return pinned_pieces;
}


template <PieceType pt>
static Bitboard
LegalSquares(Square sq, Bitboard occupied_my, Bitboard occupied)
{ return ~occupied_my & AttackSquares<pt>(sq, occupied); }


template <Color c_my, PieceType pt>
static void
AddLegalSquares(const ChessBoard& pos, MoveList& myMoves, Bitboard pinned_pieces, Bitboard valid_squares)
{
  Bitboard own_pieces = pos.piece<  c_my, ALL>();
  Bitboard occupied   = pos.All();
  Bitboard piece_bb   = pos.piece<c_my, pt>() & (~pinned_pieces);

  while (piece_bb != 0)
  {
    Square ip = NextSquare(piece_bb);
    Bitboard dest_squares  = LegalSquares<pt>(ip, own_pieces, occupied) & valid_squares;

    myMoves.Add(ip, dest_squares);
  }
}



template <Color c_my, ShifterFunc shift>
static void
PieceMovement(ChessBoard& _cb, MoveList& myMoves, int KA)
{
  myMoves.pinnedPiecesSquares  = PinnedPiecesList<c_my>(_cb, myMoves, _cb.checkers);
  Bitboard pinned_pieces = myMoves.pinnedPiecesSquares;
  Bitboard valid_squares = KA * _cb.legalSquaresMaskInCheck + (1 - KA) * AllSquares;

  PawnMovement<c_my, shift>(_cb, myMoves, pinned_pieces, KA, _cb.legalSquaresMaskInCheck);

  AddLegalSquares<c_my, BISHOP>(_cb, myMoves, pinned_pieces, valid_squares);
  AddLegalSquares<c_my, KNIGHT>(_cb, myMoves, pinned_pieces, valid_squares);
  AddLegalSquares<c_my, ROOK  >(_cb, myMoves, pinned_pieces, valid_squares);
  AddLegalSquares<c_my, QUEEN >(_cb, myMoves, pinned_pieces, valid_squares);
}

#endif

#ifndef KING_MOVE_GENERATION

template <Color c_my, PieceType pt>
Bitboard
AttackedSquaresGen(const ChessBoard& pos, Bitboard occupied)
{
  Bitboard squares = 0;
  Bitboard piece_bb = pos.piece<c_my, pt>();

  while (piece_bb != 0)
    squares |= AttackSquares<pt>(NextSquare(piece_bb), occupied);

  return squares;
}

template <Color c_my>
static Bitboard
GenerateAttackedSquares(const ChessBoard& pos)
{
  constexpr Color c_emy = ~c_my;

  Bitboard ans = 0;
  Bitboard occupied = pos.All() ^ pos.piece<c_my, KING>();

  ans |= PawnAttackSquares<c_emy>(pos);
  ans |= AttackedSquaresGen<c_emy, BISHOP>(pos, occupied);
  ans |= AttackedSquaresGen<c_emy, KNIGHT>(pos, occupied);
  ans |= AttackedSquaresGen<c_emy, ROOK  >(pos, occupied);
  ans |= AttackedSquaresGen<c_emy, QUEEN >(pos, occupied);
  ans |= AttackedSquaresGen<c_emy, KING  >(pos, occupied);

  return ans;
}


template <Color c_my, PieceType pt, PieceType emy>
static void
AddAttacker(const ChessBoard& pos, int& attackerCount, Bitboard& attackedMask)
{
  Square k_sq = SquareNo(pos.piece<c_my, KING>());
  Bitboard occupied = pos.All();
  Bitboard enemy_bb = pos.piece<~c_my, pt>() | pos.piece<~c_my, emy>();

  Bitboard king_mask  = AttackSquares<pt>(k_sq, occupied);
  Bitboard piece_mask = king_mask & enemy_bb;

  if (piece_mask == 0)
    return;

  if ((piece_mask & (piece_mask - 1)) != 0)
    ++attackerCount;

  ++attackerCount;
  attackedMask |= (king_mask & AttackSquares<pt>(SquareNo(piece_mask), occupied)) | piece_mask;
}


template <Color c_my>
static void
KingAttackers(ChessBoard& _cb)
{
  Bitboard attacked_sq = _cb.enemyAttackedSquares;
  constexpr Color c_emy = ~c_my;

  if (attacked_sq && ((_cb.piece<c_my, KING>() & attacked_sq)) == 0)
  {
    _cb.checkers = 0;
    return;
  }

  Square kpos = SquareNo(_cb.piece<c_my, KING>());

  int attackerCount = 0;
  Bitboard attackedMask = 0;

  AddAttacker<c_my, ROOK  , QUEEN>(_cb, attackerCount, attackedMask);
  AddAttacker<c_my, BISHOP, QUEEN>(_cb, attackerCount, attackedMask);
  AddAttacker<c_my, KNIGHT, NONE >(_cb, attackerCount, attackedMask);

  Bitboard pawn_sq = plt::PawnCaptureMasks[c_my][kpos] & _cb.piece<c_emy, PAWN>();
  if (pawn_sq != 0)
  {
    ++attackerCount;
    attackedMask |= pawn_sq;
  }

  _cb.checkers = attackerCount;
  _cb.legalSquaresMaskInCheck = attackedMask;
}

template <Color c_my>
static void
KingMoves(const ChessBoard& _cb, MoveList& myMoves, Bitboard attackedSq)
{
  constexpr Color c_emy = ~c_my;
  Square kpos = SquareNo(_cb.piece<c_my, KING>());
  Bitboard king_mask = plt::KingMasks[kpos];
  Bitboard dest_sq  = king_mask & (~(_cb.piece<c_my, ALL>() | attackedSq));

  myMoves.Add(kpos, dest_sq);

  if (!(_cb.csep & 1920) or ((1ULL << kpos) & attackedSq)) return;

  Bitboard Apieces = _cb.All();
  Bitboard covered_squares = Apieces | attackedSq;

  constexpr int      shift    = 56 * c_emy;
  constexpr Bitboard l_mid_sq = 2ULL << shift;
  constexpr Bitboard r_sq     = 96ULL << shift;
  constexpr Bitboard l_sq     = 12ULL << shift;

  constexpr int king_side  = 256 << (2 * c_my);
  constexpr int queen_side = 128 << (2 * c_my);

  // Can castle king_side  and no pieces are in-between
  if ((_cb.csep & king_side) and !(r_sq & covered_squares))
    dest_sq |= 1ULL << (shift + 6);

  // Can castle queen_side and no pieces are in-between
  if ((_cb.csep & queen_side) and !(Apieces & l_mid_sq) and !(l_sq & covered_squares))
    dest_sq |= 1ULL << (shift + 2);

  myMoves.Add(kpos, dest_sq);
}


#endif

#ifndef LEGAL_MOVES_CHECK

template <Color c_my>
static Bitboard
LegalPinnedPieces(const ChessBoard& _cb)
{
  constexpr Color c_emy = ~c_my;

  Square kpos  = SquareNo(_cb.piece<c_my, KING>());
  Bitboard _Ap = _cb.All();

  Bitboard erq = _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, ROOK  >();
  Bitboard ebq = _cb.piece<c_emy, QUEEN>() | _cb.piece<c_emy, BISHOP>();
  Bitboard  rq = _cb.piece<c_my , QUEEN>() | _cb.piece<c_my , ROOK  >();
  Bitboard  bq = _cb.piece<c_my , QUEEN>() | _cb.piece<c_my , BISHOP>();

  Bitboard pinned_pieces = 0;
  Bitboard ray_line = plt::LineMasks[kpos];
  Bitboard ray_diag = plt::DiagonalMasks[kpos];

  if (!((ray_line & erq) | (ray_diag & ebq)))
    return 0;

  const auto can_pinned = [&] (const auto &__f, const MaskTable& table,
        Bitboard ownP, Bitboard emyP, const char pawn) -> bool
  {
    Bitboard pieces = table[kpos] & _Ap;
    Bitboard first_piece  = __f(pieces);
    Bitboard second_piece = __f(pieces ^ first_piece);

    Square index_f = SquareNo(first_piece);
    Square index_s = SquareNo(second_piece);

    if (  !(first_piece & _cb.piece<c_my, ALL>())
      or (!(second_piece & emyP))) return false;

    pinned_pieces |= first_piece;

    if (_cb.checkers == 1) return false;

    if ((first_piece & ownP) != 0)
      return ((table[kpos] ^ table[index_s] ^ first_piece) != 0);

    Bitboard Rank63[2] = {Rank6, Rank3};

    Square eps = _cb.EnPassantSquare();;
    const auto shift = (c_my == WHITE) ? (LeftShift) : (RightShift);

    Bitboard n_pawn  = first_piece;
    Bitboard s_pawn  = (shift(n_pawn, 8) & (~_cb.All())) & Rank63[c_my];
    Bitboard free_sq = ~_cb.All();

    if (pawn == 's' && (first_piece & _cb.piece<c_my, PAWN>()))
      return ((shift(n_pawn, 8) | shift(s_pawn, 8)) & free_sq) != 0;

    if (pawn == 'c' && (first_piece & _cb.piece<c_my, PAWN>()))
    {
      Bitboard capt_sq = plt::PawnCaptureMasks[c_my][index_f];
      Bitboard dest_sq = capt_sq & second_piece;

      if (eps != 64 && (table[kpos] & capt_sq & (1ULL << eps)) != 0)
        dest_sq |= 1ULL << eps;

      return dest_sq;
    }

    return false;
  };

  if ( can_pinned(Lsb, plt::RightMasks,  rq, erq, '-')
    or can_pinned(Msb, plt::LeftMasks ,  rq, erq, '-')
    or can_pinned(Lsb, plt::UpMasks   ,  rq, erq, 's')
    or can_pinned(Msb, plt::DownMasks ,  rq, erq, 's')) return 1;

  if ( can_pinned(Lsb, plt::UpRightMasks  , bq, ebq, 'c')
    or can_pinned(Lsb, plt::UpLeftMasks   , bq, ebq, 'c')
    or can_pinned(Msb, plt::DownRightMasks, bq, ebq, 'c')
    or can_pinned(Msb, plt::DownLeftMasks , bq, ebq, 'c')) return 1;

  return pinned_pieces;
}

template <Color c_my>
static bool
LegalPawnMoves(const ChessBoard& _cb, Bitboard pinned_pieces, Bitboard atk_area)
{
  Bitboard Rank27[2] = {Rank2, Rank7};
  Bitboard Rank63[2] = {Rank6, Rank3};
  const auto shift = (_cb.color == 1) ? (LeftShift) : (RightShift);

  constexpr Color c_emy = ~c_my;
  constexpr int own = int(c_my ) << 3;
  constexpr int emy = int(c_emy) << 3;

  const auto legal_EnpassantPawns = [&] (Bitboard l_pawns, Bitboard r_pawns, Square ep)
  {
    Square kpos = SquareNo(_cb.piece<c_my, KING>());
    Bitboard ep_square = 1ULL << ep;

    if ((ep == 64) or (_cb.checkers == 1 and !(plt::PawnCaptureMasks[c_my][kpos] & _cb.piece<c_emy, PAWN>())))
      return false;

    return ((shift(l_pawns, 7 + (own >> 2)) & ep_square) and EnpassantRecheck<c_my>(ep + (2 * emy - 9), _cb))
        or ((shift(r_pawns, 7 + (emy >> 2)) & ep_square) and EnpassantRecheck<c_my>(ep + (2 * emy - 7), _cb));
  };

  const auto legal_PromotionPawns = [&] (Bitboard pawns, Bitboard move_sq, Bitboard capt_sq)
  {
    Bitboard valid_squares = 0;
    while (pawns != 0)
    {
      Square __pos = NextSquare(pawns);
      valid_squares |= (plt::PawnMasks[c_my][__pos] & move_sq)
                    | (plt::PawnCaptureMasks[c_my][__pos] & capt_sq);
    }

    return (valid_squares != 0);
  };


  Bitboard pawns   = _cb.piece<c_my, PAWN>() ^ (_cb.piece<c_my, PAWN>() & pinned_pieces);
  Bitboard e_pawns = pawns & Rank27[c_my];
  Bitboard n_pawns = pawns ^ e_pawns;
  Bitboard l_pawns = n_pawns & RightAttkingPawns;
  Bitboard r_pawns = n_pawns &  LeftAttkingPawns;
  Bitboard s_pawns = (shift(n_pawns, 8) & (~_cb.All())) & Rank63[c_my];

  Bitboard free_sq = ~_cb.All();
  Bitboard enemyP  = _cb.piece<c_emy, ALL>();
  Bitboard capt_sq = (atk_area &  enemyP) * (_cb.checkers) + ( enemyP) * (1 - _cb.checkers);
  Bitboard move_sq = (atk_area ^ capt_sq) * (_cb.checkers) + (free_sq) * (1 - _cb.checkers);


  if (legal_EnpassantPawns(l_pawns, r_pawns, _cb.EnPassantSquare()))
    return true;

  if (legal_PromotionPawns(e_pawns, move_sq, capt_sq))
    return true;

  Bitboard valid_squares = 0;

  // Capture Squares
  valid_squares |= shift(l_pawns, 7 + (own >> 2)) & capt_sq;
  valid_squares |= shift(r_pawns, 7 + (emy >> 2)) & capt_sq;

  // Non-Captures Squares
  valid_squares |= shift(s_pawns, 8) & move_sq;
  valid_squares |= shift(n_pawns, 8) & move_sq;

  return (valid_squares != 0);
}


template <Color c_my, PieceType pt>
bool CanMove
(const ChessBoard& pos, Bitboard pinned_pieces, Bitboard filter)
{
  Bitboard occupied  = pos.All();
  Bitboard pieces_my = pos.piece<c_my, ALL>();
  Bitboard piece_bb  = pos.piece<c_my, pt >() & (~pinned_pieces);

  Bitboard legal_squares = 0;

  while (piece_bb != 0)
    legal_squares |= LegalSquares<pt>(NextSquare(piece_bb), pieces_my, occupied) & filter;

  return legal_squares != 0;
}


template <Color c_my>
static bool
LegalPieceMoves(const ChessBoard &_cb)
{
  const Bitboard pinned_pieces = LegalPinnedPieces<c_my>(_cb);
  const Bitboard filter =
    _cb.checkers * _cb.legalSquaresMaskInCheck + (1 - _cb.checkers) * AllSquares;

  if (pinned_pieces & 1)
    return true;

  return LegalPawnMoves<c_my>(_cb, pinned_pieces, _cb.legalSquaresMaskInCheck)
    or CanMove<c_my, BISHOP>(_cb, pinned_pieces, filter)
    or CanMove<c_my, KNIGHT>(_cb, pinned_pieces, filter)
    or CanMove<c_my, ROOK  >(_cb, pinned_pieces, filter)
    or CanMove<c_my, QUEEN >(_cb, pinned_pieces, filter);
}

template <Color c_my>
static bool
LegalKingMoves(const ChessBoard& _cb, Bitboard attacked_squares)
{
  Square kpos = SquareNo(_cb.piece<c_my, KING>());
  Bitboard K_sq = plt::KingMasks[kpos];

  Bitboard legal_squares = K_sq & (~(_cb.piece<c_my, ALL>() | attacked_squares));

  // If no castling is possible or king is attacked by a enemy piece
  if (!(_cb.csep & 1920) or ((1ULL << kpos) & attacked_squares))
    return (legal_squares != 0);

  Bitboard Apieces = _cb.All();
  Bitboard covered_squares = Apieces | attacked_squares;

  int shift         = 56 * (_cb.color ^ 1);
  Bitboard l_mid_sq = 2ULL << shift;
  Bitboard r_sq     = 96ULL << shift;
  Bitboard l_sq     = 12ULL << shift;

  Bitboard king_side  = 256 << (2 * _cb.color);
  Bitboard queen_side = 128 << (2 * _cb.color);

  // Legal moves possible if king can castle (king or queen) side
  // and no squares between king and rook is attacked by enemy piece
  // or occupied by own piece.
  return ((_cb.csep &  king_side) and !(r_sq & covered_squares))
      or ((_cb.csep & queen_side) and !(Apieces & l_mid_sq) and !(l_sq & covered_squares));
}

#endif


#ifndef GENERATOR


/**
 * @brief Calculates the bitboard of initial squares from which a moving piece
 * could potentially give a discovered check to the opponent's king.
**/
template <Color c_my>
static Bitboard
SquaresForDiscoveredCheck(const ChessBoard& pos, MoveList& myMoves)
{
  // Implementation to calculate and return the bitboard of squares.
  // These squares are the initial positions from which a piece could move
  // to potentially give a discovered check to the enemy king.

  constexpr Color c_emy = ~c_my;

  Square k_sq = SquareNo( pos.piece<c_emy, KING>() );

  Bitboard rq = pos.piece<c_my, QUEEN>() | pos.piece<c_my, ROOK  >();
  Bitboard bq = pos.piece<c_my, QUEEN>() | pos.piece<c_my, BISHOP>();

  Bitboard occupied = pos.All();
  Bitboard occupied_my = pos.piece<c_my, ALL>();
  Bitboard calculatedBitboard = 0;

  const auto checkSquare = [&] (const auto &__f, const MaskTable& atable, Bitboard my_piece)
  {
    Bitboard mask = atable[k_sq] & occupied;
    Bitboard first_piece  = __f(mask);
    Bitboard second_piece = __f(mask ^ first_piece);

    if ((first_piece & occupied_my) and (second_piece & my_piece))
    {
      myMoves.discoverCheckMasks[SquareNo(first_piece)] = atable[k_sq] & ~atable[SquareNo(second_piece)];
      calculatedBitboard |= first_piece;
    }
  };

  if (rq)
  {
    checkSquare(Lsb, plt::RightMasks, rq);
    checkSquare(Msb, plt::LeftMasks , rq);
    checkSquare(Lsb, plt::UpMasks   , rq);
    checkSquare(Msb, plt::DownMasks , rq);
  }

  if (bq)
  {
    checkSquare(Lsb, plt::UpRightMasks  , bq);
    checkSquare(Lsb, plt::UpLeftMasks   , bq);
    checkSquare(Msb, plt::DownRightMasks, bq);
    checkSquare(Msb, plt::DownLeftMasks , bq);
  }

  return calculatedBitboard;
}

template <Color c_my>
static void
GenerateSquaresThatCheckEnemyKing(const ChessBoard& pos, MoveList& myMoves)
{
  constexpr Color c_emy = ~c_my;

  Square k_sq = SquareNo( pos.piece<c_emy, KING>() );
  Bitboard occupied = pos.All();

  myMoves.squaresThatCheckEnemyKing[0] = plt::PawnCaptureMasks[c_emy][k_sq];
  myMoves.squaresThatCheckEnemyKing[1] = AttackSquares<BISHOP>(k_sq, occupied);
  myMoves.squaresThatCheckEnemyKing[2] = AttackSquares<KNIGHT>(k_sq, occupied);
  myMoves.squaresThatCheckEnemyKing[3] = AttackSquares< ROOK >(k_sq, occupied);
  myMoves.squaresThatCheckEnemyKing[4] =
    myMoves.squaresThatCheckEnemyKing[1] | myMoves.squaresThatCheckEnemyKing[3];
}


bool
LegalMovesPresent(ChessBoard& _cb)
{
  _cb.RemoveMovegenMetadata();

  if (_cb.color == WHITE)
  {
    KingAttackers<WHITE>(_cb);
    if ((_cb.checkers < 2) and LegalPieceMoves<WHITE>(_cb))
      return true;

    _cb.enemyAttackedSquares = GenerateAttackedSquares<WHITE>(_cb);
    return LegalKingMoves<WHITE>(_cb, _cb.enemyAttackedSquares);
  }

  KingAttackers<BLACK>(_cb);
  if ((_cb.checkers < 2) and LegalPieceMoves<BLACK>(_cb))
    return true;

  _cb.enemyAttackedSquares = GenerateAttackedSquares<BLACK>(_cb);
  return LegalKingMoves<BLACK>(_cb, _cb.enemyAttackedSquares);
}


template <Color c_my, ShifterFunc shift>
static inline MoveList
MoveGenerator(ChessBoard& pos, bool generateChecksData)
{
  MoveList myMoves(c_my);

  if (!pos.EnemyAttackedSquaresGenerated())
    pos.enemyAttackedSquares = GenerateAttackedSquares<c_my>(pos);

  myMoves.myPawns = pos.piece<c_my, PAWN>();
  myMoves.enemyAttackedSquares = pos.enemyAttackedSquares;

  if (!pos.AttackersFound())
    KingAttackers<c_my>(pos);

  myMoves.checkers = pos.checkers;

  if (pos.checkers < 2)
    PieceMovement<c_my, shift>(pos, myMoves, pos.checkers);

  KingMoves<c_my>(pos, myMoves, pos.enemyAttackedSquares);

  if (generateChecksData)
  {
    myMoves.discoverCheckSquares = SquaresForDiscoveredCheck<c_my>(pos, myMoves);
    GenerateSquaresThatCheckEnemyKing<c_my>(pos, myMoves);
  }

  pos.RemoveMovegenMetadata();
  return myMoves;
}


MoveList
GenerateMoves(ChessBoard& pos, bool generateChecksData)
{
  return pos.color == WHITE ?
    MoveGenerator<WHITE,  LeftShift>(pos, generateChecksData)
  : MoveGenerator<BLACK, RightShift>(pos, generateChecksData);
}

bool
CapturesExistInPosition(const ChessBoard& pos)
{
  Bitboard attackMask = 0;
  Bitboard occupied = pos.All();

  if (pos.color == WHITE)
  {
    attackMask |= PawnAttackSquares<WHITE>(pos);
    attackMask |= AttackedSquaresGen<WHITE, BISHOP>(pos, occupied);
    attackMask |= AttackedSquaresGen<WHITE, KNIGHT>(pos, occupied);
    attackMask |= AttackedSquaresGen<WHITE, ROOK  >(pos, occupied);
    attackMask |= AttackedSquaresGen<WHITE, QUEEN >(pos, occupied);
    attackMask |= AttackedSquaresGen<WHITE, KING  >(pos, occupied);

    return (attackMask & pos.piece<BLACK, ALL>()) != 0;
  }

  attackMask |= PawnAttackSquares<BLACK>(pos);
  attackMask |= AttackedSquaresGen<BLACK, BISHOP>(pos, occupied);
  attackMask |= AttackedSquaresGen<BLACK, KNIGHT>(pos, occupied);
  attackMask |= AttackedSquaresGen<BLACK, ROOK  >(pos, occupied);
  attackMask |= AttackedSquaresGen<BLACK, QUEEN >(pos, occupied);
  attackMask |= AttackedSquaresGen<BLACK, KING  >(pos, occupied);

  return (attackMask & pos.piece<WHITE, ALL>()) != 0;
}

bool
QueenTrapped(const ChessBoard& pos, Bitboard enemyAttackedSquares)
{
  Bitboard queens      = pos.get_piece(pos.color, QUEEN);
  Bitboard occupied_my = pos.get_piece(pos.color,   ALL);
  Bitboard occupied    = pos.All();

  while (queens != 0)
  {
    Square queenSq = NextSquare(queens);
    Bitboard  mask = LegalSquares<QUEEN>(queenSq, occupied_my, occupied);

    if ((mask ^ (mask & enemyAttackedSquares)) == 0) {
      // Return true if piece attacking the queen is not opponent queen
      const PieceType pt = type_of(pos.PieceOnSquare(GetSmallestAttacker(pos, queenSq, ~pos.color, 0)));
      if (pt != QUEEN)
        return true;
    }
  }

  return false;
}

template<Color side>
Square
SmallestAttacker(const ChessBoard& pos, const Square square, Bitboard removedPieces)
{
  Bitboard occupied = pos.All() ^ removedPieces;
  Bitboard mask;

  mask = AttackSquares<~side, PAWN>(square) & (pos.piece<side, PAWN>() ^ (pos.piece<side, PAWN>() & removedPieces));
  if (mask) return MsbIndex(mask);

  mask = AttackSquares<KNIGHT>(square, occupied) & (pos.piece<side, KNIGHT>() ^ (pos.piece<side, KNIGHT>() & removedPieces));
  if (mask) return MsbIndex(mask);

  mask = AttackSquares<BISHOP>(square, occupied) & (pos.piece<side, BISHOP>() ^ (pos.piece<side, BISHOP>() & removedPieces));
  if (mask) return MsbIndex(mask);

  mask = AttackSquares<ROOK>(square, occupied) & (pos.piece<side, ROOK>() ^ (pos.piece<side, ROOK>() & removedPieces));
  if (mask) return MsbIndex(mask);

  mask = AttackSquares<QUEEN>(square, occupied) & (pos.piece<side, QUEEN>() ^ (pos.piece<side, QUEEN>() & removedPieces));
  if (mask) return MsbIndex(mask);

  mask = AttackSquares<KING>(square, occupied) & (pos.piece<side, KING>() ^ (pos.piece<side, KING>() & removedPieces));
  if (mask) return MsbIndex(mask);

  return SQUARE_NB;
}

Square
GetSmallestAttacker(const ChessBoard& pos, const Square square, Color side, Bitboard removedPieces)
{
  return side == WHITE
    ? SmallestAttacker<WHITE>(pos, square, removedPieces)
    : SmallestAttacker<BLACK>(pos, square, removedPieces);
}

#endif

