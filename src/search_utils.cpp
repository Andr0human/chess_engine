

#include "search_utils.h"

// search_data info;
SearchData info;
moveOrderClass moc;


#ifndef MOVE_ORDER_CLASS

void
moveOrderClass::initialise(MoveList& myMoves)
{
    mCount = myMoves.size();
    for (uint64_t i = 0; i < mCount; i++)
        moves[i] = std::make_pair(myMoves.pMoves[i], 0);
}

void
moveOrderClass::insert(uint64_t idx, double time_taken)
{ moves[idx].second = time_taken; }

void
moveOrderClass::sortList(int best_move)
{
    for (uint64_t i = 0; i < mCount; i++)
    {
        if (best_move == moves[i].first)
        {
            std::swap(moves[i], moves[0]);
            break;
        }
    }

    for (uint64_t i = 1; i < mCount; i++)
    {
        uint64_t idx = i;
        for (uint64_t j = i; j < mCount; j++)
            if (moves[j].second > moves[idx].second) idx = j;
        std::swap(moves[i], moves[idx]);
    }
}

void
moveOrderClass::setMoveOrder(MoveList& myMoves)
{
    for (uint64_t i = 0; i < mCount; i++) {
        int curr = moves[i].first;
        for (uint64_t j = i + 1; j < mCount; j++)
            if (myMoves.pMoves[j] == curr)
                std::swap(myMoves.pMoves[j], myMoves.pMoves[i]);
    }
}

void
moveOrderClass::mprint(chessBoard& cb)
{
    for (uint64_t i = 0; i < mCount; i++)
        cout << i + 1 << ") " << print(moves[i].first, cb)
             << " | Time : " << moves[i].second << endl;
}

int
moveOrderClass::get_move(uint64_t index)
{ return moves[index].first; }

uint64_t
moveOrderClass::moveCount()
{ return mCount; }

void
moveOrderClass::reset()
{
    for (uint64_t i = 0; i < mCount; i++)
        moves[i].second = 0;
}

#endif

/* #ifndef SEARCH_DATA

int
search_data::best_move()
{ return move[depth - 1].first; }

int
search_data::last_depth()
{ return depth - 1; }

double
search_data::last_eval()
{
    float eval = static_cast<float>(move[depth - 1].second) / 100.0f;
    return eval * static_cast<float>(2 * side2move - 1);
}

int
search_data::eval()
{ return move[depth - 1].second; }

void
search_data::set_to_move(int pc)
{ side2move = pc; }

void
search_data::update(int _d, int _e, int line[])
{
    for (int i = 0; i < _d; i++)
        pvAlpha[i] = line[i];
    move[depth++] = std::make_pair(line[0], _e);
}

void
search_data::reset()
{
    max_ply = max_qs_ply = depth = 0;
    for (int i = 0; i < maxPly; i++)
        move[i] = std::make_pair(0, 0);
    for (int i = 0; i < maxPly; i++)
        pvAlpha[i] = 0;
}

void
search_data::set_depth_zero_move(int __m)
{
    depth = 0;
    move[depth++] = std::make_pair(__m, 0);
}

bool
search_data::is_part_of_pv(int __m)
{    
    for (int i = 0; i < depth; i++)
        if (pvAlpha[i] == __m) return true;
    return false;
}

bool
search_data::use_verification_Search()
{    
    if (depth < 4) return false;
    if (last_eval() > 70) return false;

    using std::max;
    using std::min;

    auto alpha = move[depth - 1];
    auto beta  = move[depth - 2];
    auto delta = move[depth - 3];
    
    if ((alpha.first != beta.first) or (alpha.first != delta.first)) return true;

    int lower = min(min(alpha.second, beta.second), delta.second);
    int upper = max(max(alpha.second, beta.second), delta.second);

    if (upper - lower > 300) return true;
    return false;
}

bool
search_data::move_verified()
{
    if (std::abs(last_eval()) > 70) return true;
    
    std::pair<int, int> alpha = move[depth - 1], beta = move[depth - 2];

    if (alpha.first != beta.first) return false;    
    int lower = std::min(alpha.second, beta.second);
    int upper = std::max(alpha.second, beta.second);

    if (upper - lower > 300) return false;
    return true;
}

void
search_data::init(int color, int zMove)
{
    info.set_to_move(color);
    info.set_depth_zero_move(zMove);
}

void
search_data::set_discard_result(int zMove)
{
    if (zMove == -1)    
        move[depth++] = std::make_pair(zMove, negInf / 2);
    if (zMove == -2)
        move[depth++] = std::make_pair(zMove, 0);
}

#endif */

#ifndef TEST_POSITION

test_position::test_position(string f, int d, uint64_t nc, int index)
{
    fen = f;
    depth = d;
    nodeCount = nc;
    generate_name(index);
}

test_position::test_position(string f, int d, uint64_t nc, string n)
{
    fen = f;
    name = n;
    depth = d;
    nodeCount = nc;
}

void
test_position::generate_name(int index)
{
    if (index == 1) name = "pawn";
    else if (index == 2) name = "bishop";
    else if (index == 3) name = "knight";
    else if (index == 4) name = "rook";
    else if (index == 5) name = "queen";
    else name = "--";
}

void
test_position::print()
{
    cout << name << " : " << fen << "\n" << "|   Depth : " \
    << depth << "\t|   Nodes : " << nodeCount << "\t|\n" << endl;
}

#endif

#ifndef UTILs

std::vector<std::string>
split(const string &__s, char sep)
{

    std::vector<std::string> res;
    size_t prev = 0, __n = __s.length();
    
    int in_case = 0;

    for (size_t i = 0; i < __n; i++)
    {
        int val = static_cast<int>(__s[i]);
        if (val == 34 or val == 39)
            in_case ^= 1;

        if (__s[i] != sep or (in_case == 1)) continue;
        
        if (i > prev)
            res.push_back(__s.substr(prev, i - prev));
        prev = i + 1;
    }

    if (__n > prev)
        res.push_back(__s.substr(prev, __n - prev));
    return res;
}

string
strip(string __s)
{    
    if (__s.empty()) return __s;

    while (!__s.empty() &&__s.back() == ' ')
        __s.pop_back();
    
    for (size_t i = 0; i < __s.size(); i++)
        std::swap(__s[i], __s[__s.size() - 1 - i]);

    while (!__s.empty() &&__s.back() == ' ')
        __s.pop_back();
    
    for (size_t i = 0; i < __s.size(); i++)
        std::swap(__s[i], __s[__s.size() - 1 - i]);
    
    return __s;
}

vector<string>
strip(vector<string> list)
{
    for (auto &word : list) word = strip(word);
    return list;
}

vector<uint64_t>
to_nums(const vector<string>& list)
{
    vector<uint64_t> nums(list.size());
    for (size_t i = 0; i < list.size(); i++)
        nums[i] = std::stoull(list[i]);

    return nums;
}


vector<string>
extract_argument_list(int argc, char *argv[])
{
    vector<string> result(argc - 1);
    
    for (int i = 1; i < argc; i++)
        result[i - 1] = string(argv[i]);
    
    return result;
}

void
bits_on_board(uint64_t value)
{
    std::string arr[8];
    for (int i = 0; i < 8; i++) arr[i] = "........";

    while (value)
    {
        const int idx = lSb_idx(value);
        const int x = idx & 7, y = (idx - x) >> 3;
        arr[7 - y][x] = '1';
        value &= value - 1;
    }

    const string s = "+---+---+---+---+---+---+---+---+\n";
    cout << s;
    for (const auto &row : arr)
    {
        cout << "| ";
        for (const char ch : row)
            cout << ch << " | ";
        cout << "\n" << s;
    }
    cout << endl;
}

#endif

