#ifndef SEARCH_UTILS_H
#define SEARCH_UTILS_H

#include "movelist.h"


extern Move pvArray[MAX_PV_ARRAY_SIZE];
extern array<Varray<Move, KILLER_ARRAY_SIZE>, MAX_PLY> killerMoves;

// History table, [color][from][to], 2 * 64 * 64 * 4 B = 32 KB. Scores how
// often a quiet move caused a beta cutoff, weighted by depth. Used to sort the
// QUIET stage, which would otherwise be in generation order.
//
// Color is BLACK = 0, WHITE = 1, the opposite of the usual convention.
// Indexing by pos.color is fine, but an initializer or debug dump written by
// hand must not assume WHITE = 0. The compiler won't catch it.
extern array<array<array<int32_t, SQUARE_NB>, SQUARE_NB>, COLOR_NB> historyTable;


void
movcpy(Move* pTarget, const Move* pSource, int n);

void
resetPvLine();

void
clearKillers();

// Zero the history table. Called at the start of each search with
// clearKillers(), so nothing carries over between moves of a game.
void
clearHistory();

// Reward a quiet move that caused a beta cutoff at `depth`.
//
// The bonus shrinks as the entry grows, which keeps every entry inside
// (-MAX_HISTORY, MAX_HISTORY) and lets old entries fade. The usual
// alternative, adding depth*depth and halving the table when it gets too big,
// needs a pass over the whole table, and there's no good place to run one.
void
updateHistory(Color c, Move move, Depth depth);

// Penalize a quiet move that was searched at `depth` and didn't cut off, when
// a later quiet move did. Without this the table only learns which moves are
// good, not which are bad.
//
// Same update as updateHistory with the sign flipped. See the .cpp for why it
// is `-= malus + h*malus/MAX` and not `-= malus - ...`.
void
penalizeHistory(Color c, Move move, Depth depth);

// Sort key for the QUIET stage. Captures never get history updates, so a
// SEE < 0 capture that orderMoves() moved into this stage scores 0. It sorts
// below every quiet move with a positive score and above every quiet move with
// a negative one. That's intended: a bad capture is a better try than a quiet
// move that keeps failing.
inline int32_t
historyScore(Color c, Move move)
{ return historyTable[c][size_t(from_sq(move))][size_t(to_sq(move))]; }

Score
checkmateScore(Ply ply);

// True when a score lies in the mate range (mate-in-N is encoded as
// VALUE_MATE - 20*ply, so anything within 20*MAX_PLY of VALUE_MATE is a mate).
bool
isMateScore(Score score);

// Null-move search depth reduction for the given remaining depth.
int
nullReduction(Depth depth);

template <MType mt>
inline bool
is_type(Move m)
{
  if constexpr (mt == MType::CHECK)
    return (m >> 23) & 1;

  // Bits 20 (CAPTURES) and 21 (PROMOTION) only. A quiet move has neither.
  // Don't include bit 22: it's the color bit, set on every White move
  // (WHITE = 1), so none of White's moves would count as quiet.
  if constexpr (mt == MType::QUIET)
    return ((m >> 20) & 3) == 0;

  if constexpr (mt == MType::CAPTURES)
    return (m >> 20) & 1;

  if constexpr (mt == MType::PROMOTION)
    return (m >> 21) & 1;

  return 0;
}

int
rootReduction(Depth depth, size_t moveNo);

int
reduction (Depth depth, size_t moveNo);

bool
interestingMove(Move move);

bool
lmrOk(Move move, Depth depth, size_t moveNo);

int
searchExtension(
  const ChessBoard& pos,
  const MoveList& myMoves,
  int numExtensions,
  Depth depth
);


#endif


