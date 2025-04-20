
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

// Define the type for the Lsb and Msb function pointers
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
  // Splits the string using separator.
  vector<string>
  split(const string& s, char sep);

  // Strips the string of leading and trailing separators.
  string
  strip(string s, char sep = ' ');

  // Extracts the argument list from the command line.
  vector<string>
  extractArgumentList(int argc, char *argv[]);

  // Returns the string representation of the bitboard.
  string
  bitsOnBoard(Bitboard value);
}


#endif
