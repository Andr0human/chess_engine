
#ifndef BITBOARD_H
#define BITBOARD_H

#include <array>
#include "types.h"
#include "base_utils.h"
#include "tt.h"


extern uint64_t tmp_total_counter;
extern uint64_t tmp_this_counter;

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

  array<UndoInfo, 256> undoInfo;

  int undoInfoStackCounter;

  // Stores the piece at each index
  array<Piece, SQUARE_NB> board;

  // Stores bitboard location of a piece
  array<Bitboard, 16> piece_bb;

  // Stores count of each piece
  array<int, 16> piece_ct;

  // Halfmove and Fullmove
  int halfmove, fullmove;

  // MakeMove-Subparts

  bool
  IsEnpassant(Square fp, Square ep) const noexcept
  { return fp == ep; }

  bool
  IsDoublePawnPush(Square ip, Square fp) const noexcept
  { return std::abs(ip - fp) == 16; }

  bool
  IsPawnPromotion(Square fp) const noexcept
  { return (1ULL << fp) & Rank18; }

  bool
  IsCastling(Square ip, Square fp) const noexcept
  { return (ip - fp == 2) | (fp - ip == 2);}

  void
  MakeMoveCastleCheck(PieceType p, Square sq) noexcept;

  void
  MakeMoveEnpassant(Square ip, Square fp) noexcept;

  void
  MakeMoveDoublePawnPush(Square ip, Square fp) noexcept;

  void
  MakeMovePawnPromotion(Move move) noexcept;

  template <bool makeMoveCall>
  void
  MakeMoveCastling(Square ip, Square fp) noexcept;

  public:

  constexpr static array<Weight, 6> pieceValues = {
    PawnValueMg, BishopValueMg, KnightValueMg, RookValueMg, QueenValueMg, 25200
  };

  // White -> 1, Black -> 0
  Color color;

  int csep;

  Key Hash_Value;

  Weight boardWeight;

  ChessBoard();

  ChessBoard(const std::string& fen);

  void
  SetPositionWithFen(const string& fen) noexcept;

  string
  VisualBoard() const noexcept;

  void
  MakeMove(Move move, bool in_search = true) noexcept;

  void
  UnmakeMove() noexcept;

  void
  UndoInfoPush(PieceType it, PieceType ft, Move move, bool in_search);

  Move
  UndoInfoPop();

  const string
  Fen() const;

  bool
  ThreeMoveRepetition() const noexcept;

  bool
  FiftyMoveDraw() const noexcept;

  void
  AddPreviousBoardPositions(const vector<uint64_t>& prev_keys) noexcept;

  uint64_t
  GenerateHashkey() const;

  void
  MakeNullMove();

  void
  UnmakeNullMove();

  void
  Reset();

  bool operator== (const ChessBoard& other);

  bool operator!= (const ChessBoard& other);

  inline Square
  EnPassantSquare() const
  { return Square(csep & 0x7f); }

  constexpr Piece
  PieceOnSquare(Square sq) const noexcept
  { return board[sq]; }

  template <Color c, PieceType pt>
  Bitboard
  piece() const noexcept
  { return piece_bb[make_piece(c, pt)]; }

  template <Color c, PieceType pt>
  int
  count() const noexcept
  { return piece_ct[make_piece(c, pt)]; }

  template <PieceType pt>
  int
  count() const noexcept
  { return count<WHITE, pt>() + count<BLACK, pt>(); }

  Bitboard
  get_piece(Color c, PieceType pt) const noexcept
  { return piece_bb[make_piece(c, pt)]; }

  constexpr Bitboard
  All() const noexcept
  { return piece_bb[make_piece(WHITE, ALL)] | piece_bb[make_piece(BLACK, ALL)]; }

  void
  Dump(std::ostream& writer = std::cout);

  bool
  IntegrityCheck() const noexcept;

  ChessBoard
  inverse()
  {
    ChessBoard invPos;
    invPos.color = ~color;
    Bitboard initSquares = All();

    while (initSquares)
    {
      Square sq = Square(__builtin_ctzll(initSquares));
      initSquares &= initSquares - 1;

      int row = sq >> 3, col = sq & 7;
      const Square invSq = Square((7 - row) * 8 + col);
      invPos.SetPiece(invSq, make_piece(~color_of(board[sq]), type_of(board[sq])));
    }

    return invPos;
  }

  void SetPiece(Square sq, Piece p)
  {
    board[sq] = p;

    piece_bb[p] |= 1ULL << sq;
    piece_bb[(p & 8) + 7] |= 1ULL << sq;
    piece_ct[p]++;

    if ((p & 7) != KING)
      piece_ct[(p & 8) + 7]++;
  }

  void RemovePiece(Square sq, Piece p)
  {
    board[sq] = NO_PIECE;

    piece_bb[p] &= AllSquares ^ (1ULL << sq);
    piece_bb[(p & 8) + 7] &= AllSquares ^ (1ULL << sq);
    piece_ct[p]--;

    if ((p & 7) != KING)
      piece_ct[(p & 8) + 7]--;
  }
};


#endif

