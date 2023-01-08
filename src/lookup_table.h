

#ifndef LOOKUP_TABLE_H
#define LOOKUP_TABLE_H

#include <cstdint>


#define __abs(x) (x >= 0 ? x : -(x))
#define dist_btw(x1, y1, x2, y2) (__abs((x1 - x2)) + __abs((y1 - y2)))

namespace plt
{

extern uint64_t NtBoard[64], KBoard[64];
extern uint64_t uBoard[64], dBoard[64], rBoard[64], lBoard[64];
extern uint64_t urBoard[64], drBoard[64], ulBoard[64], dlBoard[64];
extern uint64_t diag_Board[64], line_Board[64];
extern uint64_t pBoard[2][64], pcBoard[2][64];
extern uint64_t d_ry[64], ad_ry[64];
// extern uint64_t LRboard[8][258], UDboard[8][258];
// extern uint64_t ad_bd[8][258], d_bd[8][258];

void
init();

}


#endif

