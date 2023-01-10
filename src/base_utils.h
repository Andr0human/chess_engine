
#ifndef BASE_UTILS_H
#define BASE_UTILS_H


#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>

using std::vector;
using std::string;
using std::cout;
using std::endl;



using MoveType  = int;
using PieceType = int;
using Square    = int;




#ifndef BIT_MANIPULATION

inline int
__ppcnt(const uint64_t N) 
{ return __builtin_popcountll(N); }

inline int
idx_no(const uint64_t N)
{ return __builtin_popcountll(N - 1); }

inline int
lSb_idx(const uint64_t N)
{ return __builtin_ctzll(N | (1ULL << 63)); }

inline int
mSb_idx(const uint64_t N)
{ return __builtin_clzll(N | 1) ^ 63; }

inline uint64_t
lSb(const uint64_t N)
{ return N ^ (N & (N - 1)); }

inline uint64_t
mSb(const uint64_t N)
{ return (N!=0) ? (1ULL << (__builtin_clzll(N)^63)) : (0); }

inline uint64_t
l_shift(const uint64_t val, const int shift)
{ return val << shift; }

inline uint64_t
r_shift(const uint64_t val, const int shift)
{ return val >> shift; }

#endif

namespace base_utils
{

vector<string>
split(const string &__s, char sep);



string
strip(string __s, char sep = ' ');



vector<string>
extract_argument_list(int argc, char *argv[]);



void
bits_on_board(uint64_t value);


}


#endif

