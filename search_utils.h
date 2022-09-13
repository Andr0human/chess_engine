

#ifndef SEARCH_UTILS_H
#define SEARCH_UTILS_H


#include "bitboard.h"
#include "movegen.h"
#include <iostream>

enum search_flag:int
{
    ofs = 7,
    maxMoves = 156, maxPly = 40,
    maxDepth = 36, default_search_time = 2,
    nullmove = 0,
    base_cap_str = 10,
    maxThreadCount = 12,
    quit = 1, opp_move = 2, self_move = 4,
    negInf = -16000, posInf = 16000, valWindow = 4, LMR_LIMIT = 4,

    valUNKNOWN = 5567899,
    HASHEMPTY = 0, HASHEXACT = 1,
    HASHALPHA = 2, HASHBETA  = 3,
};


class moveOrderClass
{
    private:
    std::pair<int, double> moves[156];
    uint64_t mCount = 0;

    public:
    void initialise(MoveList& myMoves);

    void sortList(int best_move);

    void setMoveOrder(MoveList& myMoves);

    void insert(uint64_t idx, double time_taken);

    void mprint(chessBoard &cb);

    int get_move(uint64_t index);

    uint64_t moveCount();

    void reset();
};

class search_data
{
    private:
    int depth;
    std::pair<int, int> move[maxPly];

    public:
    int side2move;
    int max_ply, max_qs_ply;
    int pvAlpha[maxPly];
    

    search_data()
    { depth = max_ply = max_qs_ply = 0; }

    int
    min3(int _X, int _Y, int _Z);

    int
    max3(int _X, int _Y, int _Z);

    int
    best_move();

    int
    last_depth();

    double
    last_eval();

    int
    eval();

    void
    set_to_move(int pc);

    void
    update(int _d, int _e, int line[]);

    void
    reset();

    void
    set_depth_zero_move(int __m);

    bool
    is_part_of_pv(int __m);

    bool
    use_verification_Search();

    bool
    move_verified();

    void
    init(int color, int zMove);

    void
    set_discard_result(int zMove);
};

struct thread_search_info
{
    chessBoard Board;
    bool beta_cut, threads_available, time_left;

    uint64_t moveCount, index;
    int depth;
    int ply, pvIndex, best_move, hashf;
    int legal_moves[156], alpha, beta;
    uint64_t NodeCount;

    thread_search_info ();

    void
    set(chessBoard &tmp_board, MoveList &tmp, int t_dep, int ta, int tb, int tply, int pv_idx, int start);

    uint64_t
    get_index();

    std::pair<int, int>
    result();
};

struct play_board
{
    chessBoard board;
    string commandline;
    static const int occ_pos_size = 300;
    uint64_t occured_positions[occ_pos_size];
    int pos_cnt, opponent_move;

    play_board()
    {
        board = chessBoard(
                string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
        pos_cnt = 0;
        opponent_move = 0;
    }

    void
    init(const string fen)
    {
        board = fen;
        pos_cnt = 0;
        opponent_move = 0;
    }

    void
    play_move(int move)
    {
        board.MakeMove(move);
        int pt = (move >> 12) & 7, cpt = (move >> 15) & 7;
        if (pt == 1 || cpt >= 1) pos_cnt = 0;                       // If pawn or a capture move, clear game history table
        occured_positions[pos_cnt++] = board.Hash_Value;            // Add Board pos. to occured table list.

        #if defined(TRANSPOSITION_TABLE_H)
            // TT.ClearTT();
            // TT.Clear_History();
        #endif
    }

    void
    generate_history_table()
    {
        #if defined(TRANSPOSITION_TABLE_H)
        // for (int i = 0; i < pos_cnt; i++)
            // TT.RecordSearch(occured_positions[i]);
        #endif
    }
};

class ponder_list
{
    void index_swap(uint64_t i, uint64_t j);

    public:
    int moves[maxMoves];
    int evals[maxMoves];
    int lines[maxMoves][maxDepth];
    uint64_t mCount;

    ponder_list() {mCount = 0;};

    void
    setList(MoveList &myMoves);

    void
    insert(int idx, int move, int eval, int line[]);

    void
    show(chessBoard &board);

    void
    sortlist();
};

class test_position
{
    public:
    string fen, name;
    int depth;
    uint64_t nodeCount = 0;

    test_position() {}

    test_position(string f, int d, uint64_t nc, string n);

    test_position(string f, int d, uint64_t nc, int index);

    void
    generate_name(int index);

    void
    print();
};


class
movegen_test_position
{
    string pos_fen;
    vector<uint64_t> nodes;

    public:
    movegen_test_position() {}

    movegen_test_position(const string& _fen, const vector<uint64_t>& _nodes)
    : pos_fen(_fen), nodes(_nodes) {}

    movegen_test_position(const string&& _fen, const vector<uint64_t>&& _nodes)
    : pos_fen(std::move(_fen)), nodes(std::move(_nodes)) {}

    uint64_t
    depth() const
    { return nodes.size(); }

    uint64_t
    expected_nodes(uint64_t depth) const
    { return nodes[depth - 1]; }

    // returns <max_depth, nodes_at_max_depth>
    const std::pair<uint64_t, uint64_t>
    max_node() const
    { return std::make_pair(nodes.size(), nodes.back()); }

    string
    fen() const
    { return pos_fen; }

    void
    print() const
    {
        cout << "Fen = " << pos_fen << '\n';
        for (size_t i = 0; i < nodes.size(); i++)
            cout << "Depth " << (i + 1) << " = " << nodes[i] << '\n';
        cout << std::endl;
    }
};



extern search_data info;
extern moveOrderClass moc;
extern thread_search_info thread_data;
extern play_board pb;
extern ponder_list pdl;

vector<string>
split(const string &__s, char sep);
string
strip(string __s);

/**
 * @brief Returns a vector of strings stripped from both ends
 * 
 * @param list vector of strings
 * @return vector<string> 
 */
vector<string>
strip(vector<string> list);
vector<uint64_t>
to_nums(const vector<string>& list);
vector<string>
extract_argument_list(int argc, char *argv[]);
void
bits_on_board(uint64_t value);


#endif


