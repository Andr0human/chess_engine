#ifndef BASE_UTILS_H
#define BASE_UTILS_H


#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>
#include "types.h"

using std::vector;
using std::string;
using std::cout;
using std::endl;

typedef uint64_t (*BitboardFunc)(uint64_t);

inline int
popCount(Bitboard __x) 
{ return __builtin_popcountll(__x); }

inline Square
squareNo(Bitboard __x)
{ return Square(__builtin_popcountll(__x - 1)) ; }

inline Square
lsbIndex(Bitboard __x)
{ return Square(__builtin_ctzll(__x | (1ULL << 63))); }

inline Square
msbIndex(Bitboard __x)
{ return Square(__builtin_clzll(__x | 1) ^ 63); }

inline Bitboard
lsb(Bitboard __x)
{ return __x ^ (__x & (__x - 1)); }

inline Bitboard
msb(Bitboard __x)
{ return (__x != 0) ? (1ULL << (__builtin_clzll(__x) ^ 63)) : (0); }

inline Bitboard
leftShift(Bitboard val, int shift)
{ return val << shift; }

inline Bitboard
rightShift(Bitboard val, int shift)
{ return val >> shift; }

namespace utils
{
  vector<string>
  split(const string& s, char sep);

  string
  strip(string s, char sep = ' ');

  vector<string>
  extractArgumentList(int argc, char *argv[]);

  string
  bitsOnBoard(Bitboard value);

  bool
  hasArg(const vector<string>& args, const string& flag);

  // Returns empty string if flag not found or has no value.
  string
  argValue(const vector<string>& args, const string& flag);

  string
  getFen(const vector<string>& args, string defaultFen);

  Depth
  getDepth(const vector<string>& args, Depth defaultDepth);

  double
  getTime(const vector<string>& args, double defaultTime);

  // Requested transposition-table size in MB. Returned as asked -- the clamping
  // and the round-down to a power-of-two entry count happen in tt.resize().
  size_t
  getHash(const vector<string>& args, size_t defaultHash);

  string
  getOutputFile(const vector<string>& args, string defaultOutputFile);

  string
  getDifficulty(const vector<string>& args, string defaultDifficulty);
}


#endif
