#ifndef SEARCH_UTILS_H
#define SEARCH_UTILS_H

#include "movelist.h"


extern Move pvArray[MAX_PV_ARRAY_SIZE];
extern array<Varray<Move, 2>, MAX_PLY> killerMoves;

// Butterfly history table, [color][from][to] — 2 * 64 * 64 * 4 B = 32 KB.
// Records how often a *quiet* move produced a beta cutoff, weighted by the
// depth it did it at, and is used to order the residual QUIET stage that
// otherwise runs in raw movegen emission order.
//
// Color indexing gotcha: Color is BLACK = 0, WHITE = 1 — inverted from the
// usual convention. Indexing by pos.color is correct (it's just an index), but
// any hand-written initializer or debug dump must not assume WHITE = 0. This
// has bitten silently before; there is no compile error for getting it wrong.
extern array<array<array<int32_t, SQUARE_NB>, SQUARE_NB>, COLOR_NB> historyTable;


void
movcpy(Move* pTarget, const Move* pSource, int n);

void
resetPvLine();

void
clearKillers();

// Zero the history table. Called per search alongside clearKillers(), so v1
// carries no state between moves of a game — keeping (or halving) it across
// moves is a separate knob, deliberately not bundled into the first A/B.
void
clearHistory();

// Reward a quiet move that produced a beta cutoff at `depth`.
//
// Gravity form: the increment shrinks as the entry grows, which bounds the
// table in (-MAX_HISTORY, MAX_HISTORY) by construction and decays stale entries
// on its own. The classic alternative — accumulate depth*depth and halve the
// whole table on overflow — would need a periodic 32 KB sweep, and there is no
// natural place in this codebase's control flow to hang one.
void
updateHistory(Color c, Move move, Depth depth);

// Ordering key for the residual QUIET stage. Never updated for captures, so a
// SEE<0 capture demoted into that band by orderMoves() scores 0 and sinks below
// every quiet that has ever cut off — which is the intended treatment.
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

  // Bits 20 (CAPTURES) and 21 (PROMOTION) only -- a quiet move is one that is
  // neither. The mask must NOT reach bit 22: that is the color bit, set on
  // every move of the side whose Color is 1, and Color is BLACK = 0,
  // WHITE = 1. Masking it in made this return false for all of White's moves,
  // which silently emptied historyTable[WHITE] and White's killer slots (the
  // sole call site gates both).
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


