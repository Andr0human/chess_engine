

#include "tt2.h"

Transposition_Table TT;

void Transposition_Table::get_random_keys() noexcept {

    std::mt19937_64 rng(1557);
    for (int i = 0; i < 860; i++) 
        HashIndex[i] = rng();

}

void Transposition_Table::free_tables() {

    if (TT_SIZE == 0)
        return;

    delete[] primary_tt_table;
    delete[] secondary_tt_table;

}

void Transposition_Table::allocate_tables() {

    primary_tt_table = new Zobrist_HashKey[TT_SIZE]();
    secondary_tt_table = new Zobrist_HashKey[TT_SIZE]();

    // ** allocate history_table

}

void Transposition_Table::resize(int preset) {
    get_random_keys();
    free_tables();
    TT_SIZE = tt_sizes[preset];
    allocate_tables();
}

std::string Transposition_Table::size() const {

    uint64_t table_size = sizeof(Zobrist_HashKey) * TT_SIZE * 2
                        /* + sizeof(HistoryNode) * HT_SIZE */;

    const uint64_t KB = 1024, MB = KB * KB, GB = MB * KB;

    if (table_size < MB)
        return std::to_string(table_size / KB) + std::string(" KB.");

    if (table_size < GB)
        return std::to_string(table_size / MB) + std::string(" MB.");

    return std::to_string(static_cast<float>(table_size) / GB) + std::string(" GB.");
}


void Transposition_Table::record_position
    (uint64_t zkey, int depth, int move, int eval, int flag) noexcept {


    const auto add_entry = [&] (Zobrist_HashKey& __pos) {
        __pos.key = zkey;
        __pos.depth = depth;
        __pos.move = move;
        __pos.eval = eval;
        __pos.flag = flag;
    };

    const uint64_t index = zkey % TT_SIZE;
    const int primary_key_depth = primary_tt_table[index].depth;

    if (depth > primary_key_depth)
        add_entry(primary_tt_table[index]);
    
    add_entry(secondary_tt_table[index]);

}


int Transposition_Table::lookup_position
    (uint64_t zkey, int depth, int alpha, int beta) const noexcept {

    const int32_t UNKNOWN_VALUE = 5567899;

    const auto lookup = [&] (const Zobrist_HashKey &__pos) {

        if (__pos.key == zkey && __pos.depth > depth) {
            if (__pos.flag == HASHEXACT) return __pos.eval;
            if (__pos.flag == HASHALPHA && __pos.eval <= alpha) return alpha;
            if (__pos.flag == HASHBETA  && __pos.eval >= beta ) return beta;
        }

        return UNKNOWN_VALUE;
    };
  
    const uint64_t index = zkey % TT_SIZE;

    int32_t res = lookup(primary_tt_table[index]);
    if (res != UNKNOWN_VALUE) return res;

    return lookup(secondary_tt_table[index]);

    // res = lookup(secondary_tt_table[index]);
    // if (res != UNKNOWN_VALUE) return res;

    // return UNKNOWN_VALUE;
}


