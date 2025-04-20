

#include "bitboard.h"

uint64_t tmpTotalCounter = 0;
uint64_t tmpThisCounter = 0;

ChessBoard::ChessBoard()
{ reset(); }

ChessBoard::ChessBoard(const string& fen)
{ setPositionWithFen(fen); }

void
ChessBoard::setPositionWithFen(const string& fen) noexcept
{
  using std::stoi;

  const auto charToPieceType = [] (char ch)
  {
    const int pieceNo[8] = {6, 2, 0, 3, 0, 1, 5, 4};

    const char piece = ch < 'a' ? ch : char(int(ch) - 32);
    int v = static_cast<int>(piece);

    int index = ((v - 3) % 10) - 2;
    return
      pieceNo[index] + (ch < 'a' ? 8 : 0);
  };

  const auto castlingRights = [&] (char ch)
  {
    if (ch == '-')
      return;

    int v = static_cast<int>(ch);
    csep |= 1 << (10 - (v % 5));
  };

  reset();

  // Split the elements from FEN.
  const vector<string> elements = utils::split(fen, ' ');

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
        Piece piece = Piece(charToPieceType(elem));
        board[square] = piece;
        pieceBb[piece] |= 1ULL << square;
        pieceBb[(piece & 8) + 7] |= 1ULL << square;
        pieceCt[piece]++;
        ++square;

        if ((piece & 7) == KING) continue;
        pieceCt[(piece & 8) + 7]++;
        boardWeight += pieceValues[type_of(piece) - 1];
      }
    }
  }

  // Extracting which color to play
  color = Color(int(elements[1][0]) & 1);

  // Extracting castle-info
  for (char ch : elements[2])
    castlingRights(ch);

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
  hashValue = generateHashkey();
}

const string
ChessBoard::fen() const
{
  const auto pieceNoToChar = [] (int pieceNo)
  {
    int pvalues[7] = {0, 80, 66, 78, 82, 81, 75};

    int v = pvalues[pieceNo & 7] + ((pieceNo & 8) ^ 8) * 4;

    return static_cast<char>(v);
  };

  const auto addZerosToFen = [] (string& s, int& zeros)
  {
    if (zeros == 0)
      return;

    s.push_back(static_cast<char>(zeros + 48));
    zeros = 0;
  };

  const auto addCastleToFen = [&] (string& s)
  {
    if ((csep & 1920) == 0)
      return s.push_back('-');

    for (int i = 0; i < 4; i++)
    {
      int state = ((i & 1) *  6)
                + ((i & 2) * 16) + 75;

      if ((1ULL << (10 - i)) & csep)
        s.push_back(static_cast<char>(state));
    }
  };

  string generatedFen;
  int zero = 0;

  for(int j = 7; j >= 0; j--)
  {
    for (int i = 0; i < 8; i++)
    {
      if (board[8 * j + i] == 0)
        zero++;
      else
      {
        addZerosToFen(generatedFen, zero);
        generatedFen.push_back(pieceNoToChar(board[8 * j + i]));
      }
    }

    addZerosToFen(generatedFen, zero);
    generatedFen.push_back('/');
  }

  generatedFen.pop_back();

  generatedFen += " ";
  generatedFen += (color == 1) ? "w " : "b ";

  addCastleToFen(generatedFen);
  generatedFen.push_back(' ');

  if ((csep & 64) != 0)
    generatedFen += "- ";
  else
  {
    generatedFen.push_back(char((csep & 7) + 97));
    generatedFen += (color == 1 ? "6 " : "3 ");
  }

  generatedFen += std::to_string(halfmove) + " " +
                  std::to_string(fullmove >> 1);

  return generatedFen;
}


void
ChessBoard::makeMove(Move move, bool inSearch) noexcept
{
  // Init and Dest. sq
  Square ip = Square(move & 63);
  Square fp = Square((move >> 6) & 63);

  Bitboard iPos = 1ULL << ip;
  Bitboard fPos = 1ULL << fp;

  Square ep = enPassantSquare();

  PieceType it = PieceType((move >> 12) & 7);
  PieceType ft = PieceType((move >> 15) & 7);

  // Piece at init and dest. sq.
  Piece ipt = board[ip];
  Piece fpt = board[fp];

  undoInfoPush(it, ft, move, inSearch);

  halfmove = (ft != NONE) or (it == PAWN) ? 0 : halfmove + 1;
  fullmove++;

  board[ip] = NO_PIECE;
  board[fp] = ipt;

  if constexpr (USE_TT) {
    if (ep != SQUARE_NB)
      hashValue ^= tt.hashKey(ep + 1);
  }

  // Reset the en-passant state
  csep = (csep & 1920) ^ SQUARE_NB;

  // Check if a rook is on dest. sq.
  makeMoveCastleCheck(ft, fp);

  // Check if a rook is on init. sq.
  makeMoveCastleCheck(it, ip);

  // Check for pawn special moves.
  if (it == PAWN)
  {
    if (isDoublePawnPush(ip, fp))
      return makeMoveDoublePawnPush(ip, fp);

    if (isEnpassant(fp, ep))
      return makeMoveEnpassant(ip, fp);

    if (isPawnPromotion(fp))
      return makeMovePawnPromotion(move);
  }

  // Check for king moves.
  if (it == KING)
  {
    int oldCsep = csep;
    int filter = 2047 ^ (384 << (color * 2));
    csep &= filter;

    if constexpr (USE_TT) {
      hashValue ^= tt.hashKey((oldCsep >> 7) + 66);
      hashValue ^= tt.hashKey((csep >> 7) + 66);
    }

    if (isCastling(ip, fp))
      return makeMoveCastling<true>(ip, fp);
  }

  if (fpt != NO_PIECE)
  {
    pieceCt[fpt]--;
    pieceBb[fpt] ^= fPos;
    pieceCt[(fpt & 8) + ALL]--;
    pieceBb[(fpt & 8) + ALL] ^= fPos;
    boardWeight -= pieceValues[ft - 1];

    if constexpr (USE_TT) {
      hashValue ^= tt.hashKeyUpdate(fpt, fp);
    }
  }

  pieceBb[ipt] ^= iPos ^ fPos;
  pieceBb[(color << 3) + 7] ^= iPos ^ fPos;

  // Flip the sides.
  color = ~color;

  if constexpr (USE_TT) {
    hashValue ^= tt.hashKeyUpdate(ipt, ip)
               ^ tt.hashKeyUpdate(ipt, fp)
               ^ tt.hashKey(0);
  }
}


void
ChessBoard::makeMoveCastleCheck(PieceType piece, Square sq) noexcept
{
  constexpr Bitboard CORNER_SQUARES = 0x8100000000000081;
  Bitboard bitPos = 1ULL << sq;

  if ((bitPos & CORNER_SQUARES) and (piece == ROOK))
  {
    int oldCsep = csep;
    int y = (sq + 1) >> 3;
    int z = y + (y < 7 ? 9 : 0);
    csep &= 2047 ^ (1 << z);

    if constexpr (USE_TT) {
      hashValue ^= tt.hashKey((oldCsep >> 7) + 66);
      hashValue ^= tt.hashKey((csep >> 7) + 66);
    }
  }
}

void
ChessBoard::makeMoveDoublePawnPush(Square ip, Square fp) noexcept
{
  int own = color << 3;
  csep = (csep & 1920) | ((ip + fp) >> 1);

  pieceBb[own + 1] ^= (1ULL << ip) ^ (1ULL << fp);
  pieceBb[own + 7] ^= (1ULL << ip) ^ (1ULL << fp);

  if constexpr (USE_TT) {
    // Add current enpassant-state to hash_value
    hashValue ^= tt.hashKey(1 + enPassantSquare());
    hashValue ^= tt.hashKeyUpdate(own + 1, ip)
               ^ tt.hashKeyUpdate(own + 1, fp)
               ^ tt.hashKey(0);
  }

  color = ~color;
}

void
ChessBoard::makeMoveEnpassant(Square ip, Square ep) noexcept
{
  int own = color << 3;
  int emy = own ^ 8;
  Square capPawnFp = ep - 8 * (2 * color - 1);

  // Remove opp. pawn from the Pieces-table
  pieceBb[emy + PAWN] ^= 1ULL << capPawnFp;
  pieceBb[emy + ALL ] ^= 1ULL << capPawnFp;
  pieceCt[emy + PAWN]--;
  pieceCt[emy + ALL ]--;
  board[capPawnFp] = NO_PIECE;
  boardWeight -= pieceValues[PAWN - 1];

  // Shift own pawn in Pieces-table
  pieceBb[own + PAWN] ^= (1ULL << ip) ^ (1ULL << ep);
  pieceBb[own +  ALL] ^= (1ULL << ip) ^ (1ULL << ep);

  color = ~color;

  if constexpr (USE_TT) {
    hashValue ^= tt.hashKeyUpdate(emy + PAWN, capPawnFp);
    hashValue ^= tt.hashKeyUpdate(own + PAWN, ip)
               ^ tt.hashKeyUpdate(own + PAWN, ep)
               ^ tt.hashKey(0);
  }
}

void
ChessBoard::makeMovePawnPromotion(Move move) noexcept
{
  Square ip = Square(move & 63);
  Square fp = Square((move >> 6) & 63);
  PieceType cpt = PieceType((move >> 15) & 7);

  int own = color << 3;
  int emy = own ^ 8;
  PieceType newPt = PieceType(((move >> 18) & 3) + 2);

  pieceBb[own + PAWN ] ^= 1ULL << ip;
  pieceCt[own + PAWN ]--;
  pieceBb[own + newPt] ^= 1ULL << fp;
  pieceCt[own + newPt]++;
  pieceBb[own + ALL  ] ^= (1ULL << ip) ^ (1ULL << fp);
  boardWeight -= pieceValues[PAWN  - 1];
  boardWeight += pieceValues[newPt - 1];

  board[fp] = make_piece(color, newPt);

  if (cpt > 0)
  {
    pieceBb[emy + cpt] ^= 1ULL << fp;
    pieceBb[emy + ALL] ^= 1ULL << fp;
    pieceCt[emy + cpt]--;
    pieceCt[emy + ALL]--;
    boardWeight -= pieceValues[cpt - 1];

    if constexpr (USE_TT) {
      hashValue ^= tt.hashKeyUpdate(emy + cpt, fp);
    }
  }

  color = ~color;

  if constexpr (USE_TT) {
    hashValue ^= tt.hashKeyUpdate(own + 1, ip);
    hashValue ^= tt.hashKeyUpdate(own + newPt, fp);
    hashValue ^= tt.hashKey(0);
  }
}

template <bool makeMoveCall>
void ChessBoard::makeMoveCastling(Square ip, Square fp) noexcept
{
  int own = int(color) << 3;

  Bitboard rooksIndexes =
      (fp > ip ? 160ULL : 9ULL) << (56 * (~color));

  pieceBb[own + 4] ^= rooksIndexes;
  pieceBb[own + 7] ^= rooksIndexes;

  const int flask = int(!makeMoveCall) * (own + 4);

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

  if constexpr (makeMoveCall)
  {
    pieceBb[own + 6] ^= (1ULL << ip) ^ (1ULL << fp);
    pieceBb[own + 7] ^= (1ULL << ip) ^ (1ULL << fp);
    color = ~color;

    if constexpr (USE_TT) {
      int p1 = lsbIndex(rooksIndexes);
      int p2 = msbIndex(rooksIndexes);
      hashValue ^= tt.hashKeyUpdate(own + 6, ip) ^ tt.hashKeyUpdate(own + 6, fp);
      hashValue ^= tt.hashKeyUpdate(own + 4, p1) ^ tt.hashKeyUpdate(own + 4, p2);
      hashValue ^= tt.hashKey(0);
    }
  }
}

void
ChessBoard::unmakeMove() noexcept
{
  if (undoInfoStackCounter <= 0) return;

  color = ~color;
  Move move = undoInfoPop();
  fullmove--;

  Square ip = Square(move & 63);
  Square fp = Square((move >> 6) & 63);
  Square ep = enPassantSquare();

  Bitboard iPos = 1ULL << ip;
  Bitboard fPos = 1ULL << fp;

  int own =  color << 3;
  int emy = ~color << 3;

  PieceType it = PieceType((move >> 12) & 7);
  PieceType ft = PieceType((move >> 15) & 7);

  Piece ipt = make_piece( color, it);
  Piece fpt = make_piece(~color, ft);

  if (fpt == 8) fpt = NO_PIECE;

  board[ip] = ipt;
  board[fp] = fpt;

  if (fpt != NO_PIECE)
  {
    pieceCt[fpt]++;
    pieceCt[(fpt & 8) + ALL]++;
    pieceBb[fpt] ^= fPos;
    pieceBb[(fpt & 8) + ALL] ^= fPos;
    boardWeight += pieceValues[ft - 1];
  }

  pieceBb[ipt] ^= iPos ^ fPos;
  pieceBb[own + 7] ^= iPos ^ fPos;

  if (it == PAWN)
  {
    if (fp == ep)
    {
      Square pawnFp = ep - 8 * (2 * color - 1);
      pieceBb[emy + PAWN] ^= 1ULL << (pawnFp);
      pieceBb[emy + ALL ] ^= 1ULL << (pawnFp);
      pieceCt[emy + PAWN]++;
      pieceCt[emy + ALL ]++;
      boardWeight += pieceValues[PAWN - 1];
      board[pawnFp] = Piece(emy + PAWN);
    }
    else if ((fPos & Rank18) != 0)
    {
      ipt = Piece((((move >> 18) & 3) + 2) + own);
      pieceBb[own + PAWN] ^= fPos;
      pieceBb[ipt] ^= fPos;
      pieceCt[own + PAWN]++;
      pieceCt[ipt]--;
      boardWeight += pieceValues[PAWN - 1];
      boardWeight -= pieceValues[(ipt & 7)  - 1];
    }
  }

  if ((it == KING) and isCastling(ip, fp))
    return makeMoveCastling<false>(ip, fp);
}


void
ChessBoard::undoInfoPush(PieceType it, PieceType ft, Move move, bool inSearch)
{
  if (!inSearch and ((ft != NONE) or (it == PAWN)))
    undoInfoStackCounter = 0;

  undoInfo[undoInfoStackCounter++] = UndoInfo(move, csep, hashValue, halfmove);
}


Move
ChessBoard::undoInfoPop()
{
  undoInfoStackCounter--;
  csep = undoInfo[undoInfoStackCounter].csep;
  hashValue = undoInfo[undoInfoStackCounter].hash;
  halfmove = undoInfo[undoInfoStackCounter].halfmove;
  return undoInfo[undoInfoStackCounter].move;
}


void
ChessBoard::addPreviousBoardPositions(const vector<Key>& prevKeys) noexcept
{
  for (Key key : prevKeys)
    undoInfo[undoInfoStackCounter++] = UndoInfo(0, 0, key, 0);
}

bool
ChessBoard::threeMoveRepetition() const noexcept
{
  int posCount = 0;
  int last = std::max(0, undoInfoStackCounter - halfmove);

  for (int i = undoInfoStackCounter - 1; i >= last; i--)
    if (hashValue == undoInfo[i].hash)
        ++posCount;

  return posCount >= 1;
}

bool
ChessBoard::fiftyMoveDraw() const noexcept
{ return halfmove >= 100; }

Key
ChessBoard::generateHashkey() const
{
  Key key = 0;

  if constexpr (USE_TT) {
    int castleOffset = 66;

    if (color == 0)
      key ^= tt.hashKey(0);

    if (enPassantSquare() != SQUARE_NB)
      key ^= tt.hashKey(enPassantSquare() + 1);

    key ^= tt.hashKey((csep >> 7) + castleOffset);

    for (int piece = 1; piece < 15; piece++)
    {
      if ((piece == 8) or (piece == 7))
        continue;

      Bitboard tmp = pieceBb[piece];
      while (tmp > 0)
      {
        Square pos = lsbIndex(tmp);
        tmp &= tmp - 1;
        key ^= tt.hashKeyUpdate(piece, pos);
      }
    }
  }

  return key;
}

void
ChessBoard::makeNullMove()
{
  csep = (csep & 1920) ^ 64;
  // hashValue ^= TT.HashIndex[0];
  color = ~color;
}

void
ChessBoard::unmakeNullMove()
{
  // Update to not use t_csep form external source
  // csep = t_csep;
  // Hash_Value ^= TT.HashIndex[0];
  color = ~color;
  // moveInvert(0, t_csep, t_HashVal);
}

void
ChessBoard::reset()
{
  for (Square i = SQ_A1; i < SQUARE_NB; ++i) board[i] = NO_PIECE;
  for (int i = 0; i < 16; i++) pieceBb[i] = 0, pieceCt[i] = 0;
  csep = 0;
  hashValue = 0;
  undoInfoStackCounter = 0;
  color = Color::WHITE;
  boardWeight = 0;

  halfmove = 0;
  fullmove = 2;
}

string
ChessBoard::visualBoard() const noexcept
{
  constexpr char pieces[16] = {
    '.', 'p', 'b', 'n', 'r', 'q', 'k', '.',
    '.', 'P', 'B', 'N', 'R', 'Q', 'K', '.'
  };

  const string s = " +---+---+---+---+---+---+---+---+\n";

  string gap = " | ";
  string res = s;

  for (Square sq = SQ_A8; sq >= SQ_A1; ++sq)
  {
    res += gap + pieces[board[sq]];
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
    if (pieceBb[i] != other.pieceBb[i]) return false;

  if (csep != other.csep) return false;
  if (color != other.color) return false;

  if (hashValue != other.hashValue) return false;

  return true;
}

bool
ChessBoard::operator!= (const ChessBoard& other)
{ return !(*this == other); }

void
ChessBoard::dump(std::ostream& writer)
{
  writer << "POSITION_DUMP" << endl;

  writer << "board: ";
  for (int i = 0; i < SQUARE_NB; i++)
    writer << board[i] << ' ';
  writer << endl;

  writer << "pieceBb: ";
  for (int i = 0; i < 16; i++)
    writer << pieceBb[i] << ' ';
  writer << endl;

  writer << "Color: " << int(color) << endl;
  writer << "csep: " << csep << endl;
  writer << "halfmove: " << halfmove << endl;
  writer << "fullmove: " << fullmove << endl;
  writer << "key: " << hashValue << endl;

  writer << "movenum: " << undoInfoStackCounter << endl;
  writer << "undoInfo: \n";

  for (int i = 0; i < undoInfoStackCounter; i++)
    writer << undoInfo[i].move << ", " << undoInfo[i].hash
           << ", " << undoInfo[i].csep << ", " << undoInfo[i].halfmove << endl;

  writer << endl;
}

bool
ChessBoard::integrityCheck() const noexcept
{
  if ((pieceBb[0] | pieceBb[8]) != 0)
    return false;

  for (Square sq = SQ_A1; sq < SQUARE_NB; ++sq)
  {
    if (board[sq] == NO_PIECE) continue;

    Piece pt = board[sq];

    if ((pieceBb[pt] & (1ULL << sq)) == 0)
      return false;
  }

  for (int side = BLACK; side <= WHITE; side++)
  {
    for (int pt = PAWN; pt <= KING; pt++)
    {
      int piece = 8 * side + pt;
      Bitboard tmp = pieceBb[piece];

      while (tmp > 0)
      {
        Square sq = lsbIndex(tmp);
        if (board[sq] != piece) return false;
        tmp &= tmp - 1;
      }
    }
  }

  return true;
}
