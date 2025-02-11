


#include "base_utils.h"


namespace base_utils
{

// Splits the string using separator.
vector<string>
Split(const string &__s, char sep)
{
    vector<string> res;
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


// String the string from start and end.
string
Strip(string __s, char sep)
{
    using std::swap;
    if (__s.empty()) return __s;

    while (!__s.empty() and (__s.back() == sep))
        __s.pop_back();
    
    for (size_t i = 0; i < __s.size() / 2; i++)
        swap(__s[i], __s[__s.size() - 1 - i]);

    while ((!__s.empty()) and (__s.back() == sep))
        __s.pop_back();
    
    for (size_t i = 0; i < __s.size() / 2; i++)
        swap(__s[i], __s[__s.size() - 1 - i]);
    
    return __s;
}


vector<string>
ExtractArgumentList(int argc, char *argv[])
{
    vector<string> result(argc - 1);
    
    for (int i = 1; i < argc; i++)
        result[i - 1] = string(argv[i]);
    
    return result;
}


string
BitsOnBoard(Bitboard value)
{
    const string s = " +---+---+---+---+---+---+---+---+\n";

    string gap = " | ";
    string res = s;

    for (int sq = 56; sq >= 0; ++sq)
    {
        string val = (1ULL << sq) & value ? "*" : ".";
        res += gap + val;
        if ((sq & 7) == 7)
        {
            sq -= 16;
            res += " |\n" + s;
        }
    }
    
    return res;
}

}

