
#include "tt.h"

TranspositionTable TT;

void TranspositionTable::allocate_table(uint64_t tt_size, uint64_t ht_size) {
    prim_hash_table = new ZobristHashKey[tt_size]();
    sec_hash_table  = new ZobristHashKey[tt_size]();
    hist_table  = new HistoryNode[ht_size]();
}

void TranspositionTable::Clear() {
    TT.ClearTT();
    TT.Clear_History();
}

void TranspositionTable::free_table() {
    delete[] prim_hash_table;
    delete[] sec_hash_table;
    delete[] hist_table;
}

void TranspositionTable::init() {
    allocate_table(HASH_TABLE_SIZE, HISTORY_TABLE_SIZE);
    Get_Random_Keys();
}

std::string TranspositionTable::hashtable_size() {

    uint64_t table_size = sizeof(ZobristHashKey) * HASH_TABLE_SIZE * 2
                        + sizeof(HistoryNode) * HISTORY_TABLE_SIZE;

    const uint64_t KB = 1024, MB = KB * KB, GB = MB * KB;

    if (table_size < MB)
        return std::to_string(table_size / KB) + " KB.";

    if (table_size < GB)
        return std::to_string(table_size / MB) + " MB.";

    return std::to_string(static_cast<float>(table_size) / GB) + " GB.";
}

void TranspositionTable::Get_Random_Keys() {

    TCHAR dir[256];
    DWORD dir_length = GetCurrentDirectory(256, dir);
    std::string file_name;
    for (size_t i = 0; i < dir_length; i++) file_name.push_back(dir[i]);
    file_name += "\\Utility\\RandomNumbers.txt";

    cout << "\nRandom numbers list dir : " << file_name << '\n';

    std::ifstream inFile;
    inFile.open(file_name);

    if (inFile.is_open()) {
        cout << "Prebuilt random numbers list found!" << "\n\n";
        for (int i = 0; i < 860; i++) inFile >> HashIndex[i];
    } else {
        cout << "Prebuilt random numbers list not found! Generating list.\n\n";
        std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        for (int i = 0; i < 860; i++) HashIndex[i] = rng();
    }

    inFile.close();
}

void TranspositionTable::reIntialize(uint64_t tt_size, uint64_t ht_size) {
    free_table();
    allocate_table(tt_size, ht_size);
    HASH_TABLE_SIZE    = tt_size;
    HISTORY_TABLE_SIZE = ht_size;
}

void TranspositionTable::ClearTT() {
    for (uint64_t i = 0; i < HASH_TABLE_SIZE; i++) {
        prim_hash_table[i].Clear();
        sec_hash_table[i].Clear();
    }
}

void TranspositionTable::ClearTT_primary() {
    for (uint64_t i = 0; i < HASH_TABLE_SIZE; i++)
        prim_hash_table[i].Clear();
}

void TranspositionTable::Clear_History() {
    for (uint64_t i = 0; i < HISTORY_TABLE_SIZE; i++)
        hist_table[i].Clear();
}

double TranspositionTable::filled_space() {
    uint64_t res = 0;
    for (uint64_t i = 0; i < HASH_TABLE_SIZE; i++)
        if (prim_hash_table[i].key) res++;
    
    return static_cast<double>(res / HASH_TABLE_SIZE * 100);
}

int TranspositionTable::ProbeHash(int depth, int alpha, int beta, uint64_t zKey) {
    ZobristHashKey *phashe = &prim_hash_table[zKey % HASH_TABLE_SIZE];

    if (phashe->key == zKey && phashe->depth >= depth) {

        if (phashe->flags == HASHEXACT) return phashe->eval;
        if (phashe->flags == HASHALPHA && phashe->eval <= alpha) return alpha;
        if (phashe->flags == HASHBETA && phashe->eval >= beta) return beta;
    }

    phashe = &sec_hash_table[zKey % HASH_TABLE_SIZE];
    if (phashe->key == zKey && phashe->depth >= depth) {
        if (phashe->flags == HASHEXACT) return phashe->eval;
        if (phashe->flags == HASHALPHA && phashe->eval <= alpha) return alpha;
        if (phashe->flags == HASHBETA && phashe->eval >= beta) return beta;
    }

    return valUNKNOWN;
}

void TranspositionTable::RecordHash(int curr_move, uint64_t zKey, int depth, int val, int t_flag) {
    ZobristHashKey *phashe = &prim_hash_table[zKey % HASH_TABLE_SIZE];
        
    if (depth > phashe->depth) {
        phashe->move  = curr_move;
        phashe->key   = zKey;
        phashe->depth = depth;
        phashe->eval  = val;
        phashe->flags = t_flag;
    }

    phashe = &sec_hash_table[zKey % HASH_TABLE_SIZE];
    phashe->move  = curr_move;
    phashe->key   = zKey;
    phashe->depth = depth;
    phashe->eval  = val;
    phashe->flags = t_flag;
}

void TranspositionTable::RecordSearch(uint64_t zKey) {
    HistoryNode *phashe = &hist_table[zKey % HISTORY_TABLE_SIZE];
    if (phashe->key == 0) {
        phashe->key = zKey;
        phashe->repeats = 1;
    }
    else if (phashe->key == zKey) phashe->repeats++;
}

void TranspositionTable::RemSearchHistory(uint64_t zKey) {
    HistoryNode *phashe = &hist_table[zKey % HISTORY_TABLE_SIZE];
    if (phashe->key == zKey) {
        phashe->repeats--;
        if (phashe->repeats <= 0) {
            phashe->repeats = 0;
            phashe->key = 0;
        }
    }
}

int TranspositionTable::ProbeSearchHistory(uint64_t zKey) {
    uint64_t idx = zKey % HISTORY_TABLE_SIZE;
    if (zKey == hist_table[idx].key && hist_table[idx].repeats >= 1) 
        return 0;

    return valUNKNOWN;
}

int TranspositionTable::Search_History_Size() {
    int res = 0;
    for (uint64_t i = 0; i < HISTORY_TABLE_SIZE; i++)
        if (hist_table[i].key) res++;
    return res;
}

ZobristHashKey TranspositionTable::hashkey_at(uint64_t zKey) {

    auto __first = prim_hash_table;
    auto __last  = prim_hash_table + HASH_TABLE_SIZE;

    for ( ;__first != __last; ++__first)
        if (__first->key == zKey) return *__first;

    return *__first;
}


