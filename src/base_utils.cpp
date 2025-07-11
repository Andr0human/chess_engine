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

  bool
  hasArg(const vector<string>& args, const string& flag)
  { return std::find(begin(args), end(args), flag) != end(args); }

  string
  argValue(const vector<string>& args, const string& flag) 
  {
    auto it = std::find(begin(args), end(args), flag);
    if (it != end(args) and ++it != end(args)) {
      return *it;
    }
    return "";
  }

  string
  getFen(const vector<string>& args, string defaultFen)
  {
    if (hasArg(args, "fen") and !argValue(args, "fen").empty())
      return argValue(args, "fen");
    return defaultFen;
  }

  Depth
  getDepth(const vector<string>& args, Depth defaultDepth)
  {
    if (hasArg(args, "depth") and !argValue(args, "depth").empty()) {
      try {
        return std::stoi(argValue(args, "depth"));
      } catch (...) {
        return defaultDepth;
      }
    }
    return defaultDepth;
  }

  double
  getTime(const vector<string>& args, double defaultTime)
  {
    if (hasArg(args, "time") and !argValue(args, "time").empty()) {
      try {
        return std::stod(argValue(args, "time"));
      } catch (...) {
        return defaultTime;
      }
    }
    return defaultTime;
  }

  string
  getOutputFile(const vector<string>& args, string defaultOutputFile)
  {
    if (hasArg(args, "output") and !argValue(args, "output").empty()) {
      return argValue(args, "output");
    }
    return defaultOutputFile;
  }

  string
  getDifficulty(const vector<string>& args, string defaultDifficulty)
  {
    if (hasArg(args, "difficulty") and !argValue(args, "difficulty").empty()) {
      return argValue(args, "difficulty");
    }
    return defaultDifficulty;
  }
}
