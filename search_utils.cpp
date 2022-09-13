

#include "search_utils.h"

search_data info;
moveOrderClass moc;
thread_search_info thread_data;
ponder_list pdl;

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

#ifndef SEARCH_DATA

int
search_data::min3(int _X, int _Y, int _Z)
{
    if (_Y < _X) _X = _Y;
    if (_Z < _X) _X = _Z;
    return _X;
}

int
search_data::max3(int _X, int _Y, int _Z)
{
    if (_Y > _X) _X = _Y;
    if (_Z > _X) _X = _Z;
    return _X;
}

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

    std::pair<int, int> alpha = move[depth - 1], beta = move[depth - 2], delta = move[depth - 3];
    
    if ((alpha.first != beta.first) || (alpha.first != delta.first)) return true;
    int lower = min3(alpha.second, beta.second, delta.second);
    int upper = max3(alpha.second, beta.second, delta.second);
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

#endif

#ifndef THREAD_SEARCH_INFO

thread_search_info::thread_search_info()
{
    threads_available = true;
    time_left = true;
    beta_cut = false;
}

void
thread_search_info::set(chessBoard &tmp_board, MoveList &tmp,
    int t_dep, int ta, int tb, int tply, int pv_idx, int start)
{
    threads_available = beta_cut = false;
    NodeCount = 0;
    Board = tmp_board;
    for (size_t i = 0; i < tmp.size(); i++)
        legal_moves[i] = tmp.pMoves[i];
    
    moveCount = tmp.size(); index = start; depth = t_dep;
    ply = tply; pvIndex = pv_idx, best_move = tmp.pMoves[0];

    hashf = HASHALPHA;
    alpha = ta; beta = tb;
}

uint64_t
thread_search_info::get_index()
{
    if (index >= moveCount) return -1;
    uint64_t value = index;
    index++;
    return value;
}

std::pair<int, int>
thread_search_info::result()
{ return std::make_pair(best_move, alpha); }

#endif

#ifndef PONDER_LIST

void
ponder_list::setList(MoveList &myMoves)
{
    mCount = myMoves.size();
    for (size_t i = 0; i < myMoves.size(); i++)
        moves[i] = myMoves.pMoves[i];
}

void
ponder_list::insert(int idx, int cmove, int ceval, int cline[])
{
    moves[idx] = cmove;
    evals[idx] = ceval;
    lines[idx][0] = cmove;
    int cnt = 0;
    while (cline[cnt])
    {
        lines[idx][cnt + 1] = cline[cnt];
        cnt++; 
    }
}

void
ponder_list::show(chessBoard &board)
{
    cout << "MoveCount : " << mCount << endl;
    chessBoard tmp;
    uint64_t cnt;

    for (uint64_t i = 0; i < mCount; i++)
    {
        cout << print(moves[i], board) << "\t| " << evals[i] << "\t| ";
        cnt = 0;
        tmp = board;

        while (lines[i][cnt])
        {
            cout << print(lines[i][cnt], tmp) << " ";
            tmp.MakeMove(lines[i][cnt]);
            cnt++;
        }
        cout << endl;
    }
}

void
ponder_list::index_swap(uint64_t i, uint64_t j)
{
    std::swap(moves[i], moves[j]);
    std::swap(evals[i], evals[j]);
    uint64_t cnt = 0;
    while (lines[i][cnt] || lines[j][cnt])
    {
        std::swap(lines[i][cnt], lines[j][cnt]);
        cnt++;
    }
}

void
ponder_list::sortlist()
{
    for (uint64_t i = 0; i < mCount; i++)
    {
        uint64_t best = i;
        for (uint64_t j = i; j < mCount; j++)
            if (evals[j] > evals[best]) best = j;
        index_swap(i, best);
    }
}

#endif

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
    
    for (size_t i = 0; i < __n; i++) {
        if (__s[i] != sep) continue;
        
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

