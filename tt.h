
#ifndef TRANSPOSITION_TABLE_H
#define TRANSPOSITION_TABLE_H


#include <windows.h>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <fstream>
#include <random>
#include <chrono>
#include <cstdint>

using std::cout;


struct ZobristHashKey {
    uint64_t key;
    int32_t depth, flags;
    int32_t eval, move;

    void Clear() {
        key = 0;
        depth = flags = 0;
        eval = move = 0;
    }

    ZobristHashKey() {
        Clear();
    }

};

struct HistoryNode {
    uint64_t key;
    int repeats;

    void Clear() {
        key = 0;
        repeats = 0;
    }

    HistoryNode() {
        Clear();
    }

};

class TranspositionTable {
    
    uint64_t HASH_TABLE_SIZE = 2189477;
    uint64_t HISTORY_TABLE_SIZE = 10061;

    ZobristHashKey *prim_hash_table, *sec_hash_table;
    HistoryNode *hist_table;

    //  2189477 -> ~100 MB Hash
    // 22508861 -> ~  1 GB Hash

    const static int valUNKNOWN = 5567899;
    const static int HASHEMPTY = 0;
    const static int HASHEXACT = 1;
    const static int HASHALPHA = 2;
    const static int HASHBETA  = 3;


    public:


    TranspositionTable() {}

    TranspositionTable(uint64_t tt_size)
    : HASH_TABLE_SIZE(tt_size) { allocate_table(tt_size, 10061); Get_Random_Keys(); }

    void init();

    void Clear();

    std::string hashtable_size();

    void allocate_table(uint64_t tt_size, uint64_t ht_size);

    void free_table();

    void Get_Random_Keys();

    void reIntialize(uint64_t tt_size, uint64_t ht_size);

    void ClearTT();

    void ClearTT_primary();

    void Clear_History();

    double filled_space();

    int ProbeHash(int depth, int alpha, int beta, uint64_t zKey);

    void RecordHash(int curr_move, uint64_t zKey, int depth, int val, int t_flag);

    void RecordSearch (uint64_t zKey);

    void RemSearchHistory (uint64_t zKey);

    int ProbeSearchHistory (uint64_t zKey);

    int Search_History_Size ();

    ZobristHashKey hashkey_at(uint64_t zKey);

    uint64_t HashIndex[860];


};

extern TranspositionTable TT;

#endif

