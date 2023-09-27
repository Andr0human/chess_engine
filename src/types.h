
#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

using     Move =      int;
using    Score =      int;
using    Depth =      int;
using      Ply =      int;
using      Key = uint64_t;
using Bitboard = uint64_t;




enum Color: int
{
    BLACK, WHITE, COLOR_NB
};


enum PieceType: int
{
    NONE, PAWN, BISHOP, KNIGHT,
    ROOK, QUEEN, KING, ALL
};


enum Piece: int
{
    NO_PIECE,
    W_PAWN = PAWN,     W_BISHOP, W_KNIGHT, W_ROOK, W_QUEEN, W_KING,
    B_PAWN = PAWN + 8, B_BISHOP, B_KNIGHT, B_ROOK, B_QUEEN, B_KING,
};


enum Search
{
    HASH_EMPTY = 0, HASH_EXACT = 1,
    HASH_ALPHA = 2, HASH_BETA  = 3,

    MAX_MOVES = 256,
    MAX_PLY = 40,
    MAX_DEPTH = 36,
    LMR_LIMIT = 4,
    EXTENSION_LIMIT = 12,
    VAL_WINDOW = 4,
    TIMEOUT = 1112223334,
    DEFAULT_SEARCH_TIME = 1,
    MAX_THREADS = 12,
    MAX_PV_ARRAY_SIZE = (MAX_PLY * (MAX_PLY + 1)) / 2,

    NULL_MOVE = 0,
};


enum Value: int
{
    VALUE_ZERO = 0,
    VALUE_DRAW = -25,
    VALUE_MATE = 16000,
    VALUE_INF  = 16001,
    VALUE_UNKNOWN = 555666777,
    VALUE_WINDOW = 4,


    PawnValueMg   = 112,  PawnValueEg   = 132,
    BishopValueMg = 312,  BishopValueEg = 336,
    KnightValueMg = 300,  KnightValueEg = 320,
    RookValueMg   = 512,  RookValueEg   = 576,
    QueenValueMg  = 926,  QueenValueEg  = 942,

    // GamePhaseLimit = 800,
    GamePhaseLimit = 8140,
};


enum Board: uint64_t
{
    Rank1 = 255ULL, Rank2 = Rank1 << 8, Rank3 = Rank1 << 16, Rank4 = Rank1 << 24,
    Rank5 = Rank1 << 32, Rank6 = Rank1 << 40, Rank7 = Rank1 << 48, Rank8 = Rank1 << 56,
    Rank18 = Rank1 | Rank8,

    FileA = 0x101010101010101, FileB = FileA << 1, FileC = FileA << 2, FileD = FileA << 3,
    FileE = FileA << 4, FileF = FileA << 5, FileG = FileA << 6, FileH = FileA << 7,

    NoSquares = 0ULL,
    AllSquares = ~NoSquares,
    WhiteSquares = 0x55AA55AA55AA55AA,
    BlackSquares = AllSquares ^ WhiteSquares,
    
    QueenSide = FileA | FileB | FileC | FileD,
    KingSide  = FileE | FileF | FileG | FileH,
    WhiteSide = Rank1 | Rank2 | Rank3 | Rank4,
    BlackSide = Rank5 | Rank6 | Rank7 | Rank8,

    RightAttkingPawns = AllSquares ^ (Rank18 | FileH),
    LeftAttkingPawns  = AllSquares ^ (Rank18 | FileA),

    CentralSquare = (Rank4 | Rank5) & (FileD | FileE),
    SemiCentralSquare = (Rank3 | Rank4 | Rank5 | Rank6) & (FileC | FileD | FileE | FileF),
    OuterSquare = AllSquares ^ SemiCentralSquare,

    WhiteColorCorner = 0xF07030180C0E0F0,
    BlackColorCorner = 0xF0E0C0800103070F,
};


enum Square : int
{
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    
    SQ_NONE,
    SQUARE_ZERO = 0,
    SQUARE_NB   = 64
};


enum MoveType
{
    NORMAL,
    CASTLING,
    CAPTURES,
    PROMOTION,
    CHECK,

    PV_MOVE = 1 << 29,
    // Quiescence Move
    QS_MOVE = 1 << 28,
};


// Toggle color
constexpr Color
operator~(Color c)
{ return Color(c ^ WHITE); }


constexpr Piece
make_piece(Color c, PieceType pt)
{ return Piece((c << 3) + pt); }


constexpr PieceType
type_of(Piece pc)
{ return PieceType(pc & 7); }


constexpr Color
color_of(Piece pc)
{ return Color(pc >> 3); }


constexpr Square from_sq(Move m)
{ return Square(m & 0x3f); }

constexpr Square to_sq(Move m)
{ return Square((m >> 6) & 0x3f); }

constexpr Move filter(Move m)
{ return m & 0x1fffff;}



constexpr Square operator+ (Square s, int d)
{ return Square(int(s) + d); }

constexpr Square operator- (Square s, int d)
{ return Square(int(s) - d); }

constexpr Square operator+= (Square& s, int d)
{ return s = s + d; }

constexpr Square operator-= (Square& s, int d)
{ return s = s - d; }

constexpr Square operator++ (Square& s)
{ return s = Square(int(s) + 1); }

constexpr Square operator-- (Square& s)
{ return s = Square(int(s) - 1); }

#endif

