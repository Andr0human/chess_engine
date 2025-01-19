
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

  void
  MakeMoveCastling(Square ip, Square fp, int call_from_makemove) noexcept;

  public:

  // White -> 1, Black -> 0
  Color color;

  int csep;
  Key Hash_Value;

  // No. of opponent pieces checking the active king
  int checkers;

  // Bitboard representing squares that the pieces of active
  // side can legally move to when the king is under check
  Bitboard legalSquaresMaskInCheck;

  // Bitboard representing squares under enemy attack
  Bitboard enemyAttackedSquares;

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

  inline bool
  EnemyAttackedSquaresGenerated() const noexcept
  { return enemyAttackedSquares != 0; }

  inline bool
  AttackersFound() const noexcept
  { return checkers != -1;  }

  inline void
  RemoveMovegenMetadata() noexcept
  {
    checkers = -1;
    legalSquaresMaskInCheck = 0;
    enemyAttackedSquares = 0;
  }

  inline Score
  HandleScore (Score score) noexcept {
    RemoveMovegenMetadata();
    return score;
  }

  constexpr bool
  InCheck() const noexcept
  { return checkers > 0; }

  constexpr Piece
  PieceOnSquare(Square sq) const noexcept
  { return board[sq]; }

  template <Color c, PieceType pt>
  Bitboard
  piece() const noexcept
  { return piece_bb[make_piece(c, pt)]; }

  template <Color c, PieceType pt>
  int
  pieceCount() const noexcept
  { return piece_ct[make_piece(c, pt)]; }

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

  void
  LoadPosition(int _board[64], Color c, string castling, string enpassant)
  {
    Reset();

    for (int i = 0; i < 64; i++) {
      board[i] = Piece(_board[i]);
    }
    
    color = c;

    for (int sq = 0; sq < 64; sq++) {
      if (board[sq] == 0) continue;
      piece_bb[board[sq]] |= 1ULL << sq;
      piece_ct[board[sq]]++;
      piece_bb[(board[sq] & 8) + 7] |= 1ULL << sq;
      piece_ct[(board[sq] & 8) + 7]++;
    }

    for (char ch : castling) {
      if (ch == '-')
        continue;

      int v = static_cast<int>(ch);
      csep |= 1 << (10 - (v % 5));
    }

    if (enpassant == "-") csep |= 64;
    else csep |= 28 + ((2 * color - 1) * 12) + (enpassant[0] - 'a');

    Hash_Value = GenerateHashkey();
  }
};


#endif

