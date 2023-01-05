

#ifndef MOVEGEN_H
#define MOVEGEN_H


#include "bitboard.h"
#include "lookup_table.h"
#include "move_utils.h"


inline MoveType gen_move_priority
       (const chessBoard &__b, const int ip, const int fp);
inline MoveType gen_pawn_move_priority
       (const chessBoard &__b, const int ip, const int fp);
inline MoveType encode_move
    (const chessBoard &__b, const int ip, const int fp, const int pr);


inline bool en_passant_recheck(const int ip, const chessBoard& _cb);
inline void Add_shift_Pawns (uint64_t endSquares, const int shift,
            const chessBoard &_cb, MoveList &myMoves);


inline void Add_Enpassant_Move(const chessBoard &_cb, MoveList &myMoves,
            const uint64_t l_pawns, const uint64_t r_pawns, const int KA);
inline void pawn_movement(const chessBoard &_cb, MoveList &myMoves,
            const uint64_t pin_pieces, const int KA, const uint64_t atk_area);
inline void promotion_pawns(const chessBoard &_cb, MoveList &myMoves,
            const uint64_t move_sq, const uint64_t capt_sq, uint64_t pawns);
            

inline uint64_t KnightMovement(int idx, const chessBoard& _cb);
inline uint64_t BishopMovement(int idx, const chessBoard& _cb);
inline uint64_t RookMovement(int idx, const chessBoard& _cb);
inline uint64_t QueenMovement(int idx, const chessBoard &_cb);
uint64_t pinned_pieces_list(const chessBoard &_cb, MoveList &myMoves, const int KA);
void piece_movement(const chessBoard &_cb, MoveList &myMoves, const int KA);


uint64_t generate_AttackedSquares(const chessBoard& _cb);
void king_attackers(chessBoard &_cb);
void KingMoves(const chessBoard& _cb, MoveList& myMoves, const uint64_t Attacked_Sq);


uint64_t can_pinned_pieces_move(const chessBoard& _cb, const int KA);
bool can_pawns_move(const chessBoard &_cb, const uint64_t pinned_pieces,
                    const int KA, const uint64_t atk_area);
uint64_t canKingMove(const chessBoard& _cb, uint64_t Attacked_Squares);


bool is_passad_pawn(int idx, chessBoard& _cb);
bool interesting_move(MoveType move, chessBoard& _cb);
bool f_prune_move(MoveType move, chessBoard& _cb);


bool has_legal_moves(chessBoard &_cb);
// MoveList generateMoves(const chessBoard &board, bool qs_only = false);
MoveList generate_moves(chessBoard &_cb, bool qs_only = false);

#endif
