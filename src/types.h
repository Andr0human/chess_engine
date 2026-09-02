
#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <limits>
#include <type_traits>
#include <string>

using     Move = uint32_t;
using    Score =  int32_t;
using    Depth =  int32_t;
using      Ply =  int32_t;
using      Key = uint64_t;
using Bitboard = uint64_t;
using    Nodes = uint64_t;
using   Weight =  int32_t;

const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Reported in the UCI `id name` line, which is how GUIs, tournament managers and
// rating lists label the engine — without it, games from two different builds are
// indistinguishable in an arena log. Bump this in the commit that gets tagged.
const std::string ENGINE_NAME = "Elsa";
const std::string ENGINE_VERSION = "2.0.0";

enum SearchFlag: bool
{
  USE_TT = true,
  USE_LMR = true,
  USE_EXTENSIONS = true,
  USE_MOVE_ORDER = true,
  USE_PVS = true,
  USE_NMP = true,
  USE_RFP = true,
  USE_RAZOR = true,
  USE_FUTILITY = true,
  USE_HISTORY = true,

  // History malus: penalize every quiet searched at a node that did NOT cut
  // off, while a later quiet did. Without it the table only ever learns which
  // moves are good, and two moves that have each cut off a few times are not
  // comparable -- a quiet that cuts off once in ten visits reads the same as
  // one that cuts off every time. Depends on USE_HISTORY; inert, not wrong,
  // with it off.
  USE_HISTORY_MALUS = true,

  USE_QSEARCH_PROMO = true,

  // Perpetual-check probe: at a node where the side to move is losing badly
  // enough that a draw would cut off, ask the standalone AND/OR prover whether
  // that side can force an unending check sequence. Proving one is a hard
  // lower bound of "draw", which is a beta cutoff in exactly that window.
  //
  // The prover fails closed (budget exhaustion returns "unknown", never a draw
  // claim), so turning this on can only ever ADD cutoffs that are true; it can
  // never invent one. What it can cost is time, which is what the gate below
  // and the arena are for.
  USE_PERPETUAL = true,
};

enum Color: uint8_t
{
  BLACK, WHITE, COLOR_NB
};

enum PieceType: uint8_t
{
  NONE, PAWN, BISHOP, KNIGHT,
  ROOK, QUEEN, KING, ALL
};

enum Piece: uint8_t
{
  NO_PIECE,
  W_PAWN = PAWN,     W_BISHOP, W_KNIGHT, W_ROOK, W_QUEEN, W_KING,
  B_PAWN = PAWN + 8, B_BISHOP, B_KNIGHT, B_ROOK, B_QUEEN, B_KING,
};

enum TableSize: uint64_t
{
  ROOK_LOOKUP_TABLE_SIZE = 106495,
  BISHOP_LOOKUP_TABLE_SIZE = 5248,
};

enum Search
{
  HASH_INDEXES_SIZE = 855,

  MAX_MOVES = 256,
  MAX_PLY = 50,
  MAX_DEPTH = 40,
  LMR_LIMIT = 4,
  // Saturation point of the butterfly history table. The gravity update
  // (h += bonus - h*bonus/MAX_HISTORY) keeps every entry inside
  // (-MAX_HISTORY, MAX_HISTORY) by construction, so there is no overflow sweep
  // to schedule. Keep it inside int16 range if the table is ever narrowed.
  MAX_HISTORY = 16384,
  EXTENSION_LIMIT = 8,
  NMP_MIN_DEPTH = 3,
  VAL_WINDOW = 16,
  RFP_MAX_DEPTH = 6,
  RAZOR_MAX_DEPTH = 3,
  FUTILITY_MAX_DEPTH = 4,
  PERPETUAL_MIN_DEPTH = 8,
  TIMEOUT = 1112223334,
  DEFAULT_SEARCH_TIME = 1,
  MAX_THREADS = 12,
  // The triangular PV rows need (MAX_PLY * (MAX_PLY + 1)) / 2 words; the +1 is a
  // spare slot that is never part of any row. quiescenceSearch writes
  // pvArray[pvIndex] = NULL_MOVE unguarded, and the pvIndex it hands its children
  // at the last row is exactly the triangular size -- one past the end. Unlike the
  // alphaBeta overflow above, that is not fixable by sizing MAX_PLY: qsearch ply is
  // bounded by capture-chain length, not by depth, so the last row is reachable for
  // any MAX_PLY. The spare word absorbs that write instead of clobbering whatever
  // follows pvArray in BSS (killerMoves).
  MAX_PV_ARRAY_SIZE = (MAX_PLY * (MAX_PLY + 1)) / 2 + 1,

  NULL_MOVE = 0,
};

// alphaBeta has no ply bound of its own: depth falls by 1 per ply, but
// searchExtension adds it back up to EXTENSION_LIMIT, so the deepest node a
// root search can reach sits at ply MAX_DEPTH + EXTENSION_LIMIT. Both per-ply
// tables are sized by MAX_PLY -- killerMoves has MAX_PLY entries, and the
// triangular pvArray's row for ply k starts at MAX_PLY*k - k*(k-1)/2, which at
// k == MAX_PLY is past the last row. Let ply reach MAX_PLY and the
// PV-update movcpy runs with source = target - 1 and n = MAX_PLY - ply - 1 =
// -1: an overlapping copy that never meets a NULL_MOVE terminator, smearing one
// move through memory until it walks off the end and faults (0xC0000005 at root
// depth 33, when MAX_PLY was 40). Keep the strict inequality.
static_assert(MAX_PLY > MAX_DEPTH + EXTENSION_LIMIT,
              "MAX_PLY must exceed MAX_DEPTH + EXTENSION_LIMIT, or an "
              "extension-saturated search indexes pvArray out of bounds");

enum class Flag: uint8_t
{
  HASH_EMPTY = 0, HASH_EXACT = 1,
  HASH_ALPHA = 2, HASH_BETA  = 3,
};

enum class Endgames: uint8_t
{
  KPK,
  KNK,
  KBK,
  KPBK,
  KPQK,
  KBNK,
  KBBK,
  KNNK,
  KRBK,
  KPNK,
  KPRK,
};

enum Value: Score
{
  VALUE_ZERO = 0,
  VALUE_DRAW = -5,
  VALUE_MATE = 16000,
  VALUE_INF  = 16001,

  // Lower edge of the mate band. Mates are encoded ply-relative at 20 points
  // per ply (checkmateScore = -VALUE_MATE + 20 * ply), so the deepest
  // representable mate (ply == MAX_PLY) scores +/-15000. Any |score| at or
  // above this is a forced mate, anything below is a normal eval. Single
  // source of truth for isMateScore() and the RFP / razoring / futility
  // mate-window gates, which used to open-code it.
  MATE_BOUND = VALUE_MATE - 20 * MAX_PLY,

  VALUE_UNKNOWN = 555666777,
  VALUE_WINDOW = 4,
  RFP_MARGIN = 110,
  RAZOR_MARGIN = 240,
  FUTILITY_MARGIN = 120,
  // How far below a draw the static eval must sit before the perpetual probe
  // is worth running. A perpetual is a SAVING resource: if the side to move is
  // already doing fine, proving it can force a draw is not news, and the
  // cutoff it produces would have come from a real move anyway.
  PERPETUAL_MARGIN = 200,
  VALUE_TRANSPOSITION_TABLE_SEED = 1557,


  PawnValueMg   =  112,  PawnValueEg   =  132,
  BishopValueMg =  352,  BishopValueEg =  392,
  KnightValueMg =  336,  KnightValueEg =  374,
  RookValueMg   =  574,  RookValueEg   =  644,
  QueenValueMg  = 1040,  QueenValueEg  = 1168,

  GamePhaseLimit = 16 * PawnValueMg + 4 * (BishopValueMg + KnightValueMg + RookValueMg) + 2 * QueenValueMg
};

enum Board: Bitboard
{
  Rank1 = 255ULL, Rank2 = Rank1 << 8, Rank3 = Rank1 << 16, Rank4 = Rank1 << 24,
  Rank5 = Rank1 << 32, Rank6 = Rank1 << 40, Rank7 = Rank1 << 48, Rank8 = Rank1 << 56,
  Rank18 = Rank1 | Rank8,

  FileA = 0x101010101010101, FileB = FileA << 1, FileC = FileA << 2, FileD = FileA << 3,
  FileE = FileA << 4, FileF = FileA << 5, FileG = FileA << 6, FileH = FileA << 7,
  FileAH = FileA | FileH,

  NoSquares = 0ULL,
  AllSquares = ~NoSquares,
  WhiteSquares = 0x55AA55AA55AA55AA,
  BlackSquares = AllSquares ^ WhiteSquares,

  RightAttkingPawns = AllSquares ^ (Rank18 | FileH),
  LeftAttkingPawns  = AllSquares ^ (Rank18 | FileA),
  CornerSquares = FileAH & Rank18,
  EdgeSquares   = FileAH | Rank18
};

enum Square: int8_t
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

enum class MType: uint8_t
{
  QUIET     = 1,
  CAPTURES  = 1 << 1,
  CHECK     = 1 << 2,
  PROMOTION = 1 << 3,
  PV = 1 << 4,
  KILLER = 1 << 5,
};

/**
 * @brief Stages of the move-generation pipeline.
 *
 * GEN_METADATA - attacked-square bitboards, checkers count, in-check mask
 *                (and stamps the active color onto the MoveList).
 * GEN_MOVES    - the actual moves (pins, pawns, sliders/knights, king + castling).
 * GEN_CHECKS   - discovered-check / check-giving-square data (search-only).
 *
 * GEN_METADATA must run first; GEN_MOVES depends on it. GEN_CHECKS is independent
 * of GEN_MOVES and only reads the board.
 */
enum MoveGenStage { GEN_METADATA, GEN_MOVES, GEN_CHECKS };


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
{ return m & 0x7fffff;}

constexpr Move quiescenceMove()
{ return Move(1) << (std::numeric_limits<Move>::digits - 1); }

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

constexpr MType operator|(MType lhs, MType rhs)
{
  return static_cast<MType>(
    static_cast<std::underlying_type_t<MType>>(lhs) |
    static_cast<std::underlying_type_t<MType>>(rhs)
  );
}

constexpr MType operator&(MType lhs, MType rhs)
{
  return static_cast<MType>(
    static_cast<std::underlying_type_t<MType>>(lhs) &
    static_cast<std::underlying_type_t<MType>>(rhs)
  );
}

constexpr bool hasFlag(MType value, MType flag)
{ return static_cast<std::underlying_type_t<MType>>(value & flag) != 0; }

#endif

