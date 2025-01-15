

#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H

#include "types.h"
#include <iostream>
#include <random>
#include <array>

using std::array;

class ZobristHashKey
{
    public:
    uint64_t key;
    int32_t depth, move;
    int32_t eval, flag;

    ZobristHashKey() noexcept
    { Clear(); }

    void
    Clear() noexcept
    {
        key = 0;
        depth = move = 0;
        eval = flag = 0;
    }

    void
    show() const noexcept
    {
        std::cout 
            << "Key = " << key << '\n'
            << "Depth = " << depth << '\n'
            << "Move = " << move << '\n'
            << "Eval = " << eval << '\n'
            << "Flag = " << flag << std::endl;
    }
};

class TranspositionTable
{
    /**
     * 0 -> 100 MB table_size
     * 1 ->   1 GB table_size 
     */
    array<uint64_t, 2> tt_sizes = { 2189477ULL, 22508861ULL };

    uint64_t TT_SIZE = 0;

    ZobristHashKey *primary_tt_table;
    ZobristHashKey *secondary_tt_table;

    array<uint64_t, HASH_INDEXES_SIZE> HashIndex;

    void GetRandomKeys() noexcept;

    void AllocateTables();

    void FreeTables();

    public:
    // Initialize Transposition Table
    TranspositionTable() { }

    TranspositionTable(int preset)
    { resize(preset); }

    void
    resize(int preset = 0);

    std::string
    size() const noexcept;

    void
    Clear() noexcept;

    uint64_t
    HashKey(int __pos) const noexcept
    { return HashIndex[__pos]; }

    uint64_t
    HashkeyUpdate(int piece, int __pos) const noexcept;

    void
    RecordPosition(uint64_t zkey, Depth depth, Move move, Score eval, int flag) noexcept;
    
    int
    LookupPosition(uint64_t zkey, Depth depth, Score alpha, Score beta) const noexcept;
};


extern TranspositionTable TT;

#endif


