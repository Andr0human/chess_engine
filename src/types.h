
#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

using     Move =      int;
using   Square =      int;
using    Score =      int;
using    Depth =      int;
using      Key = uint64_t;
using Bitboard = uint64_t;


enum Color: int
{
    BLACK, WHITE
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

    GamePhaseLimit = 800,
};



enum Board: uint64_t
{
    Rank1 = 255ULL, Rank2 = Rank1 << 8, Rank3 = Rank1 << 16, Rank4 = Rank1 << 24,
    Rank5 = Rank1 << 32, Rank6 = Rank1 << 40, Rank7 = Rank1 << 48, Rank8 = Rank1 << 56,
    Rank18 = Rank1 | Rank8,

    FileA = 0x101010101010101, FileB = FileA << 1, FileC = FileA << 2, FileD = FileA << 3,
    FileE = FileA << 4, FileF = FileA << 5, FileG = FileA << 6, FileH = FileA << 7,

    dg_row  = 0x102040810204080,
    adg_row = 0x8040201008040201, 

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


#endif

