
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



#ifndef BIT_MANIPULATION

inline int
PopCount(Bitboard __x) 
{ return __builtin_popcountll(__x); }

inline Square
SquareNo(Bitboard __x)
{ return Square(__builtin_popcountll(__x - 1)) ; }

inline Square
LsbIndex(Bitboard __x)
{ return Square(__builtin_ctzll(__x | (1ULL << 63))); }

inline Square
MsbIndex(Bitboard __x)
{ return Square(__builtin_clzll(__x | 1) ^ 63); }

inline Bitboard
Lsb(Bitboard __x)
{ return __x ^ (__x & (__x - 1)); }

inline Bitboard
Msb(Bitboard __x)
{ return (__x != 0) ? (1ULL << (__builtin_clzll(__x) ^ 63)) : (0); }

inline Bitboard
LeftShift(Bitboard val, int shift)
{ return val << shift; }

inline Bitboard
RightShift(Bitboard val, int shift)
{ return val >> shift; }

#endif

namespace base_utils
{

vector<string>
Split(const string& __s, char sep);



string
Strip(string __s, char sep = ' ');



vector<string>
ExtractArgumentList(int argc, char *argv[]);



void
BitsOnBoard(uint64_t value);


}


#endif

