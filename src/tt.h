

#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H


#include <cstdint>
#include <iostream>
#include <random>


class Zobrist_HashKey
{
    public:
    uint64_t key;
    int32_t depth, move;
    int32_t eval, flag;

    Zobrist_HashKey() noexcept
    { clear(); }

    void
    clear() noexcept
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

class Transposition_Table
{
    const static int HASHEMPTY = 0;
    const static int HASHEXACT = 1;
    const static int HASHALPHA = 2;
    const static int HASHBETA  = 3;
    const static int HASH_INDEXES_SIZE = 855;

    /**
     * 0 -> 100 MB table_size
     * 1 ->   1 GB table_size 
     */
    const uint64_t tt_sizes[2] = {
        2189477ULL, 22508861ULL
    };

    uint64_t TT_SIZE = 0;

    Zobrist_HashKey *primary_tt_table;
    Zobrist_HashKey *secondary_tt_table;

    uint64_t HashIndex[HASH_INDEXES_SIZE];    

    void get_random_keys() noexcept;

    void allocate_tables();

    void free_tables();

    public:
    // Initialize Transposition Table
    Transposition_Table() { }

    Transposition_Table(int preset)
    { resize(preset); }

    void
    resize(int preset = 0);

    std::string
    size() const noexcept;

    void
    clear() noexcept;

    uint64_t
    hash_key(int __pos) const noexcept
    { return HashIndex[__pos]; }

    uint64_t
    hashkey_update(int piece, int __pos) const noexcept;

    void
    record_position(uint64_t zkey, int depth, int move, int eval, int flag) noexcept;
    
    int
    lookup_position(uint64_t zkey, int depth, int alpha, int beta) const noexcept;
};


extern Transposition_Table TT;

#endif


