

#ifndef SEARCH_UTILS_H
#define SEARCH_UTILS_H


#include <iomanip>
#include "perf.h"
#include "bitboard.h"
#include "movegen.h"

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

    TIMEOUT = 1112223334,
    DRAW_VALUE = -25,


    valUNKNOWN = 5567899,
    HASHEMPTY = 0, HASHEXACT = 1,
    HASHALPHA = 2, HASHBETA  = 3,
};



class MoveOrderClass
{
    private:
    std::pair<int, double> moves[156];
    uint64_t mCount = 0;

    public:
    void
    initialise(MoveList& myMoves);

    void
    sortList(int best_move);

    void
    setMoveOrder(MoveList& myMoves);

    void
    insert(uint64_t idx, double time_taken);

    void
    mprint(ChessBoard &cb);

    int
    get_move(uint64_t index);

    uint64_t
    moveCount();

    void
    reset();
};



class SearchData
{
    private:
    // Set the starting point for clock
    perf_clock StartTime;

    // Store which side to play for search_position
    int side;

    // Time provided to find move for current position (in secs.)
    std::chrono::nanoseconds time_alloted;

    // Time spend on searching for move in position (in secs.)
    double time_on_search;

    // Stores the pv of the lastest searched depth
    vector<MoveType> last_pv;

    // Stores the <best_move, eval> for each depth during search.
    vector<std::pair<MoveType, int>> move_eval;


    static string
    readable_pv_line(ChessBoard board, const vector<MoveType>& pv) noexcept
    {
        string res;
        for (const MoveType move : pv)
        {
            if (legal_move_for_position(move, board) == false)
                break;
            res += print(move, board) + string(" ");
            board.MakeMove(move);
        }
        return res;
    }

    public:
    // Init
    SearchData()
    : StartTime(perf::now()) {}


    SearchData(ChessBoard& pos, double _time_alloted)
    {
        StartTime = perf::now();
        side = pos.color;
        std::chrono::duration<double> dur_obj(_time_alloted);
        time_alloted = std::chrono::duration_cast<std::chrono::nanoseconds>(dur_obj);

        // A Zero depth Move is produced in case we
        // don't have time to do a search of depth 1
        MoveType zeroMove = generate_moves(pos).pMoves[0];
        move_eval = {std::make_pair(zeroMove, 0)};
    }
    

    std::pair<MoveType, int> last_iter_result() const noexcept
    { return move_eval.back(); }

    MoveType
    last_move() const noexcept
    { return move_eval.back().first; }

    size_t
    last_depth() const noexcept
    { return move_eval.size() - 1; }

    vector<MoveType>
    last_pv_line() const noexcept
    { return last_pv; }

    bool
    is_part_of_pv(MoveType move) const noexcept
    {
        for (MoveType pv_move : last_pv)
            if (move == pv_move) return true;
        return false;
    }

    bool
    time_over() noexcept
    {
        std::chrono::nanoseconds duration = perf::now() - StartTime;
        return duration >= time_alloted;
    }

    void
    add_current_depth_result(size_t depth, int eval, int __pv[]) noexcept
    {
        last_pv.clear();
        for (size_t i = 0; i < depth; i++)
            last_pv.emplace_back(__pv[i]);

        move_eval.emplace_back(std::make_pair(__pv[0], eval * (2 * side - 1)));
    }

    void
    search_completed() noexcept
    {
        perf_time duration = perf::now() - StartTime;
        time_on_search = duration.count();
    }

    double
    search_time() const noexcept
    { return time_on_search; }

    void
    set_discard_result(MoveType zero_move) noexcept
    {
        int eval = (zero_move == -1) ? (negInf / 2) :(0);
        move_eval.emplace_back(std::make_pair(zero_move, eval));
    }

    // Prints the result of search_iterative
    string
    get_search_results(ChessBoard board)
    {
        const auto&[move, eval] = last_iter_result();
        const double eval_conv = static_cast<double>(eval) / 100.0;

        string result = "------ SEARCH-INFO ------";

        result +=
          + "\nDepth Searched = " + std::to_string(move_eval.size() - 1)
          + "\nBest Move = " + print(move, board)
          + "\nEval = " + std::to_string(eval_conv)
          + "\nLine = " + readable_pv_line(board, last_pv)
          + "\nTime_Spend = " + std::to_string(time_on_search) + " sec."
          + "\n-------------------------";
        
        return result;
    }

    // Prints the results of last searched depth
    string
    show_last_depth_result(ChessBoard board) const noexcept
    {
        // cout << std::setprecision(2);
    
        const size_t depth = (move_eval.size() - 1);
        const auto& [move, eval] = move_eval.back();
        const double eval_conv = static_cast<double>(eval) / 100.0;
        const string gap = (depth < 10) ? (" ") : ("");

        string result = "Depth " + gap + std::to_string(depth) + " | ";

        result +=
            "Eval = " + std::to_string(eval_conv) + "\t| "
          + "Pv = " + readable_pv_line(board, last_pv);
        return result;
    }
};



class TestPosition
{
    public:
    string fen, name;
    int depth;
    uint64_t nodeCount = 0;

    TestPosition() {}

    TestPosition(string f, int d, uint64_t nc, string n);

    TestPosition(string f, int d, uint64_t nc, int index);

    void
    generate_name(int index);

    void
    print();
};



class MovegenTestPosition
{
    string pos_fen;
    vector<uint64_t> nodes;

    public:
    MovegenTestPosition() {}

    MovegenTestPosition(const string& _fen, const vector<uint64_t>& _nodes)
    : pos_fen(_fen), nodes(_nodes) {}

    MovegenTestPosition(const string&& _fen, const vector<uint64_t>&& _nodes)
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


extern SearchData info;
extern MoveOrderClass moc;


#endif


