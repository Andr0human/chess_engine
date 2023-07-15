
#ifndef BASE_UTILS_H
#define BASE_UTILS_H


#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cstdint>

using std::vector;
using std::string;
using std::cout;
using std::endl;



using MoveType  = int;
using PieceType = int;
using Square    = int;




#ifndef BIT_MANIPULATION

inline int
__ppcnt(const uint64_t __x) 
{ return __builtin_popcountll(__x); }

inline int
idx_no(const uint64_t __x)
{ return __builtin_popcountll(__x - 1); }

inline int
lSb_idx(const uint64_t __x)
{ return __builtin_ctzll(__x | (1ULL << 63)); }

inline int
mSb_idx(const uint64_t __x)
{ return __builtin_clzll(__x | 1) ^ 63; }

inline uint64_t
lSb(const uint64_t __x)
{ return __x ^ (__x & (__x - 1)); }

inline uint64_t
mSb(const uint64_t __x)
{ return (__x != 0) ? (1ULL << (__builtin_clzll(__x) ^ 63)) : (0); }

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

