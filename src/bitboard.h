
#ifndef BITBOARD_H
#define BITBOARD_H

#include <array>
#include "types.h"
#include "base_utils.h"
#include "tt.h"


extern uint64_t tmpTotalCounter;
extern uint64_t tmpThisCounter;

using std::array;


class UndoInfo
{
  public:
  // Last Move
  Move move;

  // Last castle and en-passant states.
  int csep;

  // Last Position Hash
  Key hash;

  // Last HalfMove
  int halfmove;

  UndoInfo() : move(0), csep(0), hash(0), halfmove(0) {}

  UndoInfo(Move m, int c, Key h, int hm)
  : move(m), csep(c), hash(h), halfmove(hm) {}
};


class ChessBoard
{
  private:

  constexpr static array<Weight, 6> pieceValues = {
    PawnValueMg, BishopValueMg, KnightValueMg, RookValueMg, QueenValueMg, 25200
  };

  array<UndoInfo, 256> undoInfo;

  int undoInfoStackCounter;

  // Stores the piece at each index
  array<Piece, SQUARE_NB> board;

  // Stores bitboard location of a piece
  array<Bitboard, 16> pieceBb;

  // Stores count of each piece
  array<int, 16> pieceCt;

  // Halfmove and Fullmove
  int halfmove, fullmove;

  // MakeMove-Subparts

  bool
  isEnpassant(Square fp, Square ep) const noexcept
  { return fp == ep; }

  bool
  isDoublePawnPush(Square ip, Square fp) const noexcept
  { return std::abs(ip - fp) == 16; }

  bool
  isPawnPromotion(Square fp) const noexcept
  { return (1ULL << fp) & Rank18; }

  bool
  isCastling(Square ip, Square fp) const noexcept
  { return (ip - fp == 2) | (fp - ip == 2);}

  void
  makeMoveCastleCheck(PieceType p, Square sq) noexcept;

  void
  makeMoveEnpassant(Square ip, Square fp) noexcept;

  void
  makeMoveDoublePawnPush(Square ip, Square fp) noexcept;

  void
  makeMovePawnPromotion(Move move) noexcept;

  template <bool makeMoveCall>
  void
  makeMoveCastling(Square ip, Square fp) noexcept;

  public:

  // White -> 1, Black -> 0
  Color color;

  int csep;

  Key hashValue;

  Weight boardWeight;

  ChessBoard();

  ChessBoard(const string& fen);

  void
  setPositionWithFen(const string& fen) noexcept;

  string
  visualBoard() const noexcept;

  void
  makeMove(Move move, bool inSearch = true) noexcept;

  void
  unmakeMove() noexcept;

  void
  undoInfoPush(PieceType it, PieceType ft, Move move, bool inSearch);

  Move
  undoInfoPop();

  const string
  fen() const;

  bool
  threeMoveRepetition() const noexcept;

  bool
  fiftyMoveDraw() const noexcept;

  void
  addPreviousBoardPositions(const vector<uint64_t>& prevKeys) noexcept;

  uint64_t
  generateHashkey() const;

  void
  makeNullMove();

  void
  unmakeNullMove();

  void
  reset();

  bool operator== (const ChessBoard& other);

  bool operator!= (const ChessBoard& other);

  inline Square
  enPassantSquare() const
  { return Square(csep & 0x7f); }

  constexpr Piece
  pieceOnSquare(Square sq) const noexcept
  { return board[sq]; }

  template <Color c, PieceType pt>
  Bitboard
  piece() const noexcept
  { return pieceBb[make_piece(c, pt)]; }

  template <Color c, PieceType pt>
  int
  count() const noexcept
  { return pieceCt[make_piece(c, pt)]; }

  template <PieceType pt>
  int
  count() const noexcept
  { return count<WHITE, pt>() + count<BLACK, pt>(); }

  Bitboard
  getPiece(Color c, PieceType pt) const noexcept
  { return pieceBb[make_piece(c, pt)]; }

  constexpr Bitboard
  all() const noexcept
  { return pieceBb[make_piece(WHITE, ALL)] | pieceBb[make_piece(BLACK, ALL)]; }

  void
  dump(std::ostream& writer = std::cout);

  bool
  integrityCheck() const noexcept;

  ChessBoard
  inverse()
  {
    ChessBoard invPos;
    invPos.color = ~color;
    Bitboard initSquares = all();

    while (initSquares)
    {
      Square sq = Square(__builtin_ctzll(initSquares));
      initSquares &= initSquares - 1;

      int row = sq >> 3, col = sq & 7;
      const Square invSq = Square((7 - row) * 8 + col);
      invPos.setPiece(invSq, make_piece(~color_of(board[sq]), type_of(board[sq])));
    }

    return invPos;
  }

  void
  setPiece(Square sq, Piece p)
  {
    board[sq] = p;

    pieceBb[p] |= 1ULL << sq;
    pieceBb[(p & 8) + 7] |= 1ULL << sq;
    pieceCt[p]++;

    if ((p & 7) != KING)
      pieceCt[(p & 8) + 7]++;
  }

  void
  removePiece(Square sq, Piece p)
  {
    board[sq] = NO_PIECE;

    pieceBb[p] &= AllSquares ^ (1ULL << sq);
    pieceBb[(p & 8) + 7] &= AllSquares ^ (1ULL << sq);
    pieceCt[p]--;

    if ((p & 7) != KING)
      pieceCt[(p & 8) + 7]--;
  }

  Weight
  pieceValue(PieceType pt) const noexcept
  { return pieceValues[pt - 1]; }
};


#endif

