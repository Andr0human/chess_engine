#include "bitboard.h"
#include "attacks.h"
#include "evaluation.h"

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

bool
isValid(const ChessBoard& pos)
{
  const Bitboard wPawns = pos.piece<WHITE, PAWN>();
  const Bitboard bPawns = pos.piece<BLACK, PAWN>();

  const Bitboard wKing = pos.piece<WHITE, KING>();
  const Bitboard bKing = pos.piece<BLACK, KING>();

  const Square wKingSq = SquareNo(wKing);
  const Square bKingSq = SquareNo(bKing);

  if ((plt::KingMasks[wKingSq] & bKing) or (plt::KingMasks[bKingSq] & wKing))
    return false;

  if (((wPawns | bPawns) & Rank18))
    return false;

  Bitboard attackMask = 0;
  Bitboard occupied = pos.All();

  if (pos.color == WHITE)
  {
    attackMask |= PawnAttackSquares<WHITE>(pos);
    attackMask |= AttackedSquaresGen<WHITE, BISHOP>(pos, occupied);
    attackMask |= AttackedSquaresGen<WHITE, KNIGHT>(pos, occupied);
    attackMask |= AttackedSquaresGen<WHITE, ROOK  >(pos, occupied);
    attackMask |= AttackedSquaresGen<WHITE, QUEEN >(pos, occupied);

    return (attackMask & bKing) == 0;
  }

  attackMask |= PawnAttackSquares<BLACK>(pos);
  attackMask |= AttackedSquaresGen<BLACK, BISHOP>(pos, occupied);
  attackMask |= AttackedSquaresGen<BLACK, KNIGHT>(pos, occupied);
  attackMask |= AttackedSquaresGen<BLACK, ROOK  >(pos, occupied);
  attackMask |= AttackedSquaresGen<BLACK, QUEEN >(pos, occupied);
  
  return (attackMask & wKing) == 0;
}

uint64_t createPos(const Piece piece[], int b[], int n, int cnt = 0)
{
  if (cnt == n) {
    ChessBoard pos;
    pos.LoadPosition(b, WHITE, "-", "-");

    return (isValid(pos) and !isTheoreticalDraw(pos)) ? 1 : 0;
  }

  uint64_t answer = 0;

  for (int i = 0; i < 64; i++) {
    if (b[i] != 0) continue;

    b[i] = piece[cnt];
    answer += createPos(piece, b, n, cnt + 1);
    b[i] = 0;
  }

  return answer;
}

void test()
{
  // KPK = Total: 163328 | DRAW: 159 | NON-DRAWN: 163169

  int n = 3;
  const Piece pieces[] = {make_piece(WHITE, KING), make_piece(BLACK, KING), make_piece(WHITE, PAWN)};
  int board[64]{};

  uint64_t result = createPos(pieces, board, n);
  cout << "Result: " << result << endl;
}

