

#include "bitboard.h"

uint64_t tmp_total_counter = 0;
uint64_t tmp_this_counter = 0;

ChessBoard::ChessBoard()
{ Reset(); }

ChessBoard::ChessBoard(const string& fen)
{ SetPositionWithFen(fen); }

void
ChessBoard::SetPositionWithFen(const string& fen) noexcept
{
  using std::stoi;

  const auto char_to_piece_type = [] (char ch)
  {
    const int piece_no[8] = {6, 2, 0, 3, 0, 1, 5, 4};

    const char piece = ch < 'a' ? ch : char(int(ch) - 32);
    int v = static_cast<int>(piece);

    int index = ((v - 3) % 10) - 2;
    return
      piece_no[index] + (ch < 'a' ? 8 : 0);
  };

  const auto castling_rights = [&] (char ch)
  {
    if (ch == '-')
      return;

    int v = static_cast<int>(ch);
    csep |= 1 << (10 - (v % 5));
  };

  Reset();

  // Split the elements from FEN.
  const vector<string> elements = base_utils::Split(fen, ' ');

  {
    // Generating board and Pieces array

    Square square = SQ_A8;
    for (const char elem : elements[0])
    {
      if (isdigit(elem))
        square += static_cast<int>(elem - '0');
      else if (elem == '/')
        square -= (square & 7) ? ((square & 7) + 8) : (16);
      else
      {
        Piece __x = Piece(char_to_piece_type(elem));
        board[square] = __x;
        piece_bb[__x] |= 1ULL << square;
        piece_bb[(__x & 8) + 7] |= 1ULL << square;
        piece_ct[__x]++;
        ++square;

        if ((__x & 7) == KING) continue;
        piece_ct[(__x & 8) + 7]++;
        boardWeight += pieceValues[type_of(__x) - 1];
      }
    }
  }

  // Extracting which color to play
  color = Color(int(elements[1][0]) & 1);

  // Extracting castle-info
  for (char ch : elements[2])
    castling_rights(ch);

  // Extracting en-passant square
  if (elements[3] == "-") csep |= 64;
  else csep |= 28 + ((2 * color - 1) * 12) + (elements[3][0] - 'a');

  // Extracting half-move and full-move
  if (elements.size() == 6)
  {
    halfmove = stoi(elements[4]);
    fullmove = stoi(elements[5]) * 2 + (color ^ 1);
  }

  // Generate hash-value for current position
  Hash_Value = GenerateHashkey();
}

const string
ChessBoard::Fen() const
{
  const auto piece_no_to_char = [] (int piece_no)
  {
    int pvalues[7] = {0, 80, 66, 78, 82, 81, 75};

    int v = pvalues[piece_no & 7]
      + ((piece_no & 8) ^ 8) * 4;

    return static_cast<char>(v);
  };

  const auto add_zeros_to_fen = [] (string& __s, int& zeros)
  {
    if (zeros == 0)
      return;

    __s.push_back(static_cast<char>(zeros + 48));
    zeros = 0;
  };

  const auto add_castle_to_fen = [&] (string& __s)
  {
    if ((csep & 1920) == 0)
      return __s.push_back('-');

    for (int i = 0; i < 4; i++)
    {
      int state = ((i & 1) *  6)
                + ((i & 2) * 16) + 75;

      if ((1ULL << (10 - i)) & csep)
        __s.push_back(static_cast<char>(state));
    }
  };

  string generated_fen;
  int zero = 0;

  for(int j = 7; j >= 0; j--)
  {
    for (int i = 0; i < 8; i++)
    {
      if (board[8 * j + i] == 0)
        zero++;
      else
      {
        add_zeros_to_fen(generated_fen, zero);
        generated_fen.push_back(piece_no_to_char(board[8 * j + i]));
      }
    }

    add_zeros_to_fen(generated_fen, zero);
    generated_fen.push_back('/');
  }

  generated_fen.pop_back();

  generated_fen += " ";
  generated_fen += (color == 1) ? "w " : "b ";

  add_castle_to_fen(generated_fen);
  generated_fen.push_back(' ');

  if ((csep & 64) != 0)
    generated_fen += "- ";
  else
  {
    generated_fen.push_back(char((csep & 7) + 97));
    generated_fen += (color == 1 ? "6 " : "3 ");
  }

  generated_fen += std::to_string(halfmove) + " " +
                   std::to_string(fullmove >> 1);

  return generated_fen;
}


void
ChessBoard::MakeMove(Move move, bool in_search) noexcept
{
  // Init and Dest. sq
  Square ip = Square(move & 63);
  Square fp = Square((move >> 6) & 63);

  Bitboard iPos = 1ULL << ip;
  Bitboard fPos = 1ULL << fp;

  Square ep = EnPassantSquare();

  PieceType it = PieceType((move >> 12) & 7);
  PieceType ft = PieceType((move >> 15) & 7);

  // Piece at init and dest. sq.
  Piece ipt = board[ip];
  Piece fpt = board[fp];

  UndoInfoPush(it, ft, move, in_search);

  halfmove = (ft != NONE) or (it == PAWN) ? 0 : halfmove + 1;
  fullmove++;

  board[ip] = NO_PIECE;
  board[fp] = ipt;

  if constexpr (useTT) {
    if (ep != SQUARE_NB)
      Hash_Value ^= TT.HashKey(ep + 1);
  }

  // Reset the en-passant state
  csep = (csep & 1920) ^ SQUARE_NB;

  // Check if a rook is on dest. sq.
  MakeMoveCastleCheck(ft, fp);

  // Check if a rook is on init. sq.
  MakeMoveCastleCheck(it, ip);

  // Check for pawn special moves.
  if (it == PAWN)
  {
    if (IsDoublePawnPush(ip, fp))
      return MakeMoveDoublePawnPush(ip, fp);

    if (IsEnpassant(fp, ep))
      return MakeMoveEnpassant(ip, fp);

    if (IsPawnPromotion(fp))
      return MakeMovePawnPromotion(move);
  }

  // Check for king moves.
  if (it == KING)
  {
    int old_csep = csep;
    int filter = 2047 ^ (384 << (color * 2));
    csep &= filter;

    if constexpr (useTT) {
      Hash_Value ^= TT.HashKey((old_csep >> 7) + 66);
      Hash_Value ^= TT.HashKey((csep >> 7) + 66);
    }

    if (IsCastling(ip, fp))
      return MakeMoveCastling(ip, fp, 1);
  }

  if (fpt != NO_PIECE)
  {
    piece_ct[fpt]--;
    piece_bb[fpt] ^= fPos;
    piece_ct[(fpt & 8) + ALL]--;
    piece_bb[(fpt & 8) + ALL] ^= fPos;
    boardWeight -= pieceValues[ft - 1];

    if constexpr (useTT) {
      Hash_Value ^= TT.HashkeyUpdate(fpt, fp);
    }
  }

  piece_bb[ipt] ^= iPos ^ fPos;
  piece_bb[(color << 3) + 7] ^= iPos ^ fPos;

  // Flip the sides.
  color = ~color;

  if constexpr (useTT) {
    Hash_Value ^= TT.HashkeyUpdate(ipt, ip)
                ^ TT.HashkeyUpdate(ipt, fp)
                ^ TT.HashKey(0);
  }
}


void
ChessBoard::MakeMoveCastleCheck(PieceType piece, Square sq) noexcept
{
  // piece - at (init) or (dest) square.

  constexpr Bitboard CORNER_SQUARES = 0x8100000000000081;
  Bitboard bit_pos = 1ULL << sq;

  if ((bit_pos & CORNER_SQUARES) and (piece == ROOK))
  {
    int old_csep = csep;
    int y = (sq + 1) >> 3;
    int z  = y + (y < 7 ? 9 : 0);
    csep &= 2047 ^ (1 << z);

    if constexpr (useTT) {
      Hash_Value ^= TT.HashKey((old_csep >> 7) + 66);
      Hash_Value ^= TT.HashKey((csep >> 7) + 66);
    }
  }
}

void
ChessBoard::MakeMoveDoublePawnPush(Square ip, Square fp) noexcept
{
  int own = color << 3;
  csep = (csep & 1920) | ((ip + fp) >> 1);

  piece_bb[own + 1] ^= (1ULL << ip) ^ (1ULL << fp);
  piece_bb[own + 7] ^= (1ULL << ip) ^ (1ULL << fp);

  if constexpr (useTT) {
    // Add current enpassant-state to hash_value
    Hash_Value ^= TT.HashKey(1 + EnPassantSquare());
    Hash_Value ^= TT.HashkeyUpdate(own + 1, ip)
                ^ TT.HashkeyUpdate(own + 1, fp)
                ^ TT.HashKey(0);
  }

  color = ~color;
}

void
ChessBoard::MakeMoveEnpassant(Square ip, Square ep) noexcept
{
  int own = color << 3;
  int emy = own ^ 8;
  Square cap_pawn_fp = ep - 8 * (2 * color - 1);

  // Remove opp. pawn from the Pieces-table
  piece_bb[emy + PAWN] ^= 1ULL << cap_pawn_fp;
  piece_bb[emy + ALL ] ^= 1ULL << cap_pawn_fp;
  piece_ct[emy + PAWN]--;
  piece_ct[emy + ALL ]--;
  board[cap_pawn_fp] = NO_PIECE;
  boardWeight -= pieceValues[PAWN - 1];

  // Shift own pawn in Pieces-table
  piece_bb[own + PAWN] ^= (1ULL << ip) ^ (1ULL << ep);
  piece_bb[own + ALL] ^= (1ULL << ip) ^ (1ULL << ep);

  color = ~color;

  if constexpr (useTT) {
    Hash_Value ^= TT.HashkeyUpdate(emy + PAWN, cap_pawn_fp);
    Hash_Value ^= TT.HashkeyUpdate(own + PAWN, ip)
                ^ TT.HashkeyUpdate(own + PAWN, ep)
                ^ TT.HashKey(0);
  }
}

void
ChessBoard::MakeMovePawnPromotion(Move move) noexcept
{
  Square ip  = Square(move & 63);
  Square fp  = Square((move >> 6) & 63);
  PieceType cpt = PieceType((move >> 15) & 7);

  int own = color << 3;
  int emy = own ^ 8;
  PieceType new_pt = PieceType(((move >> 18) & 3) + 2);

  piece_bb[own + PAWN  ] ^= 1ULL << ip;
  piece_ct[own + PAWN  ]--;
  piece_bb[own + new_pt] ^= 1ULL << fp;
  piece_ct[own + new_pt]++;
  piece_bb[own + ALL   ] ^= (1ULL << ip) ^ (1ULL << fp);
  boardWeight -= pieceValues[PAWN   - 1];
  boardWeight += pieceValues[new_pt - 1];

  board[fp] = make_piece(color, new_pt);

  if (cpt > 0)
  {
    piece_bb[emy + cpt] ^= 1ULL << fp;
    piece_bb[emy + ALL] ^= 1ULL << fp;
    piece_ct[emy + cpt]--;
    piece_ct[emy + ALL]--;
    boardWeight -= pieceValues[cpt - 1];

    if constexpr (useTT) {
      Hash_Value ^= TT.HashkeyUpdate(emy + cpt, fp);
    }
  }

  color = ~color;

  if constexpr (useTT) {
    Hash_Value ^= TT.HashkeyUpdate(own + 1, ip);
    Hash_Value ^= TT.HashkeyUpdate(own + new_pt, fp);
    Hash_Value ^= TT.HashKey(0);
  }
}

void
ChessBoard::MakeMoveCastling(Square ip, Square fp, int call_from_makemove) noexcept
{
  int own = int(color) << 3;

  Bitboard rooks_indexes =
      (fp > ip ? 160ULL : 9ULL) << (56 * (~color));

  piece_bb[own + 4] ^= rooks_indexes;
  piece_bb[own + 7] ^= rooks_indexes;

  int flask = (!call_from_makemove) * (own + 4);

  if (fp > ip)
  {
    board[ip + 3] = Piece(flask);
    board[ip + 1] = Piece((own + 4) ^ flask);
  }
  else
  {
    board[ip - 4] = Piece(flask);
    board[ip - 1] = Piece((own + 4) ^ flask);
  }

  if (call_from_makemove)
  {
    piece_bb[own + 6] ^= (1ULL << ip) ^ (1ULL << fp);
    piece_bb[own + 7] ^= (1ULL << ip) ^ (1ULL << fp);
    color = ~color;

    if constexpr (useTT) {
      int __p1 = LsbIndex(rooks_indexes);
      int __p2 = MsbIndex(rooks_indexes);
      Hash_Value ^= TT.HashkeyUpdate(own + 6, ip) ^ TT.HashkeyUpdate(own + 6, fp);
      Hash_Value ^= TT.HashkeyUpdate(own + 4, __p1) ^ TT.HashkeyUpdate(own + 4, __p2);
      Hash_Value ^= TT.HashKey(0);
    }
  }
}

void
ChessBoard::UnmakeMove() noexcept
{
  if (undoInfoStackCounter <= 0) return;

  color = ~color;
  Move move = UndoInfoPop();
  fullmove--;

  Square ip = Square(move & 63);
  Square fp = Square((move >> 6) & 63);
  Square ep = EnPassantSquare();

  Bitboard iPos = 1ULL << ip;
  Bitboard fPos = 1ULL << fp;

  int own =  color << 3;
  int emy = (~color) << 3;

  PieceType it = PieceType((move >> 12) & 7);
  PieceType ft = PieceType((move >> 15) & 7);

  Piece ipt = make_piece( color, it);
  Piece fpt = make_piece(~color, ft);

  if (fpt == 8) fpt = NO_PIECE;

  board[ip] = ipt;
  board[fp] = fpt;

  if (fpt != NO_PIECE)
  {
    piece_ct[fpt]++;
    piece_ct[(fpt & 8) + ALL]++;
    piece_bb[fpt] ^= fPos;
    piece_bb[(fpt & 8) + ALL] ^= fPos;
    boardWeight += pieceValues[ft - 1];
  }

  piece_bb[ipt] ^= iPos ^ fPos;
  piece_bb[own + 7] ^= iPos ^ fPos;

  if (it == PAWN)
  {
    if (fp == ep)
    {
      Square pawn_fp = ep - 8 * (2 * color - 1);
      piece_bb[emy + PAWN] ^= 1ULL << (pawn_fp);
      piece_bb[emy + ALL ] ^= 1ULL << (pawn_fp);
      piece_ct[emy + PAWN]++;
      piece_ct[emy + ALL ]++;
      boardWeight += pieceValues[PAWN - 1];
      board[pawn_fp] = Piece(emy + PAWN);
    }
    else if ((fPos & Rank18) != 0)
    {
      ipt = Piece((((move >> 18) & 3) + 2) + own);
      piece_bb[own + PAWN] ^= fPos;
      piece_bb[ipt] ^= fPos;
      piece_ct[own + PAWN]++;
      piece_ct[ipt]--;
      boardWeight += pieceValues[PAWN - 1];
      boardWeight -= pieceValues[(ipt & 7)  - 1];
    }
  }

  if ((it == KING) and IsCastling(ip, fp))
    return MakeMoveCastling(ip, fp, 0);
}


void
ChessBoard::UndoInfoPush(PieceType it, PieceType ft, Move move, bool in_search)
{
  if (!in_search and ((ft != NONE) or (it == PAWN)))
    undoInfoStackCounter = 0;

  undoInfo[undoInfoStackCounter++] = UndoInfo(move, csep, Hash_Value, halfmove);
}


Move
ChessBoard::UndoInfoPop()
{
  undoInfoStackCounter--;
  csep = undoInfo[undoInfoStackCounter].csep;
  Hash_Value = undoInfo[undoInfoStackCounter].hash;
  halfmove = undoInfo[undoInfoStackCounter].halfmove;
  return undoInfo[undoInfoStackCounter].move;
}


void
ChessBoard::AddPreviousBoardPositions(const vector<Key>& prev_keys) noexcept
{
  for (Key key : prev_keys)
    undoInfo[undoInfoStackCounter++] = UndoInfo(0, 0, key, 0);
}

bool
ChessBoard::ThreeMoveRepetition() const noexcept
{
  int pos_count = 0;
  int last = std::max(0, undoInfoStackCounter - halfmove);

  for (int i = undoInfoStackCounter - 1; i >= last; i--)
    if (Hash_Value == undoInfo[i].hash)
        ++pos_count;

  return pos_count >= 1;
}

bool
ChessBoard::FiftyMoveDraw() const noexcept
{ return halfmove >= 100; }

Key
ChessBoard::GenerateHashkey() const
{
  Key key = 0;

  if constexpr (useTT) {
    int castle_offset = 66;
    if (color == 0)
      key ^= TT.HashKey(0);

    if (EnPassantSquare() != SQUARE_NB)
      key ^= TT.HashKey(EnPassantSquare() + 1);

    key ^= TT.HashKey((csep >> 7) + castle_offset);

    for (int piece = 1; piece < 15; piece++)
    {
      if ((piece == 8) or (piece == 7))
        continue;

      Bitboard __tmp = piece_bb[piece];
      while (__tmp > 0)
      {
        Square __pos = LsbIndex(__tmp);
        __tmp &= __tmp - 1;
        key ^= TT.HashkeyUpdate(piece, __pos);
      }
    }
  }

  return key;
}

void
ChessBoard::MakeNullMove()
{
  UndoInfoPush(NONE, NONE, NULL_MOVE, true);

  csep = (csep & 1920) ^ 64;
  ++halfmove;

  if (useTT)
    Hash_Value ^= TT.HashKey(0);

  color = ~color;
}

void
ChessBoard::UnmakeNullMove()
{
  if (undoInfoStackCounter <= 0) return;
  UndoInfoPop();
  color = ~color;
}

void
ChessBoard::Reset()
{
  for (Square i = SQ_A1; i < SQUARE_NB; ++i) board[i] = NO_PIECE;
  for (int i = 0; i < 16; i++) piece_bb[i] = 0, piece_ct[i] = 0;
  csep = 0;
  Hash_Value = 0;
  undoInfoStackCounter = 0;
  color = Color::WHITE;
  boardWeight = 0;

  halfmove = 0;
  fullmove = 2;
}

string
ChessBoard::VisualBoard() const noexcept
{
  constexpr char _piece[16] = {
    '.', 'p', 'b', 'n', 'r', 'q', 'k', '.',
    '.', 'P', 'B', 'N', 'R', 'Q', 'K', '.'
  };

  const string s = " +---+---+---+---+---+---+---+---+\n";

  string gap = " | ";
  string res = s;

  for (Square sq = SQ_A8; sq >= SQ_A1; ++sq)
  {
    res += gap + _piece[board[sq]];
    if ((sq & 7) == 7)
    {
      sq -= 16;
      res += " |\n" + s;
    }
  }
  return res;
}

bool
ChessBoard::operator==(const ChessBoard& other)
{
  for (Square i = SQ_A1; i < SQUARE_NB; ++i)
    if (board[i] != other.board[i]) return false;

  for (int i = 0; i < 16; i++)
    if (piece_bb[i] != other.piece_bb[i]) return false;

  if (csep != other.csep) return false;
  if (color != other.color) return false;

  if (Hash_Value != other.Hash_Value) return false;

  return true;
}

bool
ChessBoard::operator!= (const ChessBoard& other)
{ return !(*this == other); }

void
ChessBoard::Dump(std::ostream& writer)
{
  writer << "POSITION_DUMP" << endl;

  writer << "board: ";
  for (int i = 0; i < 64; i++)
    writer << board[i] << ' ';
  writer << endl;

  writer << "piece_bb: ";
  for (int i = 0; i < 16; i++)
    writer << piece_bb[i] << ' ';
  writer << endl;

  writer << "Color: " << int(color) << endl;
  writer << "csep: " << csep << endl;
  writer << "halfmove: " << halfmove << endl;
  writer << "fullmove: " << fullmove << endl;
  writer << "key: " << Hash_Value << endl;

  writer << "movenum: " << undoInfoStackCounter << endl;
  writer << "undoInfo: \n";

  for (int i = 0; i < undoInfoStackCounter; i++)
    writer << undoInfo[i].move << ", " << undoInfo[i].hash
           << ", " << undoInfo[i].csep << ", " << undoInfo[i].halfmove << endl;

  writer << endl;
}

bool
ChessBoard::IntegrityCheck() const noexcept
{
  if ((piece_bb[0] | piece_bb[8]) != 0)
    return false;

  for (Square sq = SQ_A1; sq < SQUARE_NB; ++sq)
  {
    if (board[sq] == NO_PIECE) continue;

    Piece pt = board[sq];

    if ((piece_bb[pt] & (1ULL << sq)) == 0)
      return false;
  }

  for (int side = BLACK; side <= WHITE; side++)
  {
    for (int pt = PAWN; pt <= KING; pt++)
    {
      int piece = 8 * side + pt;
      Bitboard p_bb = piece_bb[piece];

      while (p_bb > 0)
      {
        Square sq = LsbIndex(p_bb);
        if (board[sq] != piece) return false;
        p_bb &= p_bb - 1;
      }
    }
  }

  return true;
}
