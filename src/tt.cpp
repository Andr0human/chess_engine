

#include "tt.h"

TranspositionTable TT;

//! TODO xorshift for Random HashKey Generation
uint64_t
xorshift64star(void)
{
    /* initial seed must be nonzero, don't use a static variable for the state if multithreaded */
    static uint64_t x = 1237;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 0x2545F4914F6CDD1DULL;
}

void
TranspositionTable::get_random_keys() noexcept
{
    std::mt19937_64 rng(1557);
    for (int i = 0; i < HASH_INDEXES_SIZE; i++) 
        HashIndex[i] = rng();
}

void
TranspositionTable::free_tables()
{
    if (TT_SIZE == 0)
        return;

    delete[] primary_tt_table;
    delete[] secondary_tt_table;
}

void
TranspositionTable::allocate_tables()
{
    primary_tt_table = new ZobristHashKey[TT_SIZE]();
    secondary_tt_table = new ZobristHashKey[TT_SIZE]();
}

void
TranspositionTable::resize(int preset)
{
    get_random_keys();
    free_tables();
    TT_SIZE = tt_sizes[preset];
    allocate_tables();
}

std::string
TranspositionTable::size() const noexcept
{
    uint64_t table_size = sizeof(ZobristHashKey) * TT_SIZE * 2;

    uint64_t KB = 1024, MB = KB * KB, GB = MB * KB;

    if (table_size < MB)
        return std::to_string(table_size / KB) + std::string(" KB.");

    if (table_size < GB)
        return std::to_string(table_size / MB) + std::string(" MB.");

    return std::to_string(static_cast<float>(table_size) / static_cast<float>(GB)) + std::string(" GB.");
}

uint64_t
TranspositionTable::hashkey_update
    (int piece, int __pos) const noexcept
{
    int offset = 85;
    int color = piece >> 3;
    piece = (piece & 7) - 1;

    return HashIndex[ offset + __pos
         + 64 * (piece + (6 * color)) ];
}

void
TranspositionTable::record_position
    (uint64_t zkey, int depth, int move, int eval, int flag) noexcept
{
    const auto add_entry = [&] (ZobristHashKey& __pos)
    {
        __pos.key = zkey;
        __pos.depth = depth;
        __pos.move = move;
        __pos.eval = eval;
        __pos.flag = flag;
    };

    uint64_t index = zkey % TT_SIZE;
    int primary_key_depth = primary_tt_table[index].depth;

    if (depth > primary_key_depth)
        add_entry(primary_tt_table[index]);
    
    add_entry(secondary_tt_table[index]);
}


int
TranspositionTable::lookup_position
    (uint64_t zkey, int depth, int alpha, int beta) const noexcept
{
    int UNKNOWN_VALUE = 5567899;

    const auto lookup = [&] (const ZobristHashKey &__pos)
    {
        if (__pos.key == zkey && __pos.depth > depth) {
            if (__pos.flag == HASHEXACT) return __pos.eval;
            if (__pos.flag == HASHALPHA && __pos.eval <= alpha) return alpha;
            if (__pos.flag == HASHBETA  && __pos.eval >= beta ) return beta;
        }

        return UNKNOWN_VALUE;
    };
  
    uint64_t index = zkey % TT_SIZE;

    int res = lookup(primary_tt_table[index]);
    if (res != UNKNOWN_VALUE) return res;

    return lookup(secondary_tt_table[index]);
}


void
TranspositionTable::clear() noexcept
{
    for (size_t i = 0; i < TT_SIZE; i++)
    {
        primary_tt_table[i].clear();
        secondary_tt_table[i].clear();
    }
}

