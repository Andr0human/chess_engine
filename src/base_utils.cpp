#include "base_utils.h"

namespace utils
{
  vector<string>
  split(const string& s, char sep)
  {
    vector<string> res;
    size_t prev = 0, n = s.length();

    int in_case = 0;

    for (size_t i = 0; i < n; i++)
    {
      int val = static_cast<int>(s[i]);
      if (val == 34 or val == 39)
        in_case ^= 1;

      if (s[i] != sep or (in_case == 1)) continue;

      if (i > prev)
        res.push_back(s.substr(prev, i - prev));
      prev = i + 1;
    }

    if (n > prev)
      res.push_back(s.substr(prev, n - prev));
    return res;
  }

  string
  strip(string s, char sep)
  {
    using std::swap;
    if (s.empty()) return s;

    while (!s.empty() and (s.back() == sep))
      s.pop_back();

    for (size_t i = 0; i < s.size() / 2; i++)
      swap(s[i], s[s.size() - 1 - i]);

    while ((!s.empty()) and (s.back() == sep))
      s.pop_back();

    for (size_t i = 0; i < s.size() / 2; i++)
      swap(s[i], s[s.size() - 1 - i]);

    return s;
  }

  vector<string>
  extractArgumentList(int argc, char *argv[])
  {
    vector<string> result(argc - 1);
    
    for (int i = 1; i < argc; i++)
      result[i - 1] = string(argv[i]);

    return result;
  }

  string
  bitsOnBoard(Bitboard value)
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
