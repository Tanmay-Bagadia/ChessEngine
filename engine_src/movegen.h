#pragma once
#include "position.h"
#include <vector>

using Move = uint16_t;

extern U64 knight_attacks[64];
extern U64 king_attacks[64];
extern U64 rook_masks[64];
extern U64 bishop_masks[64];
extern U64 rook_magic[64];
extern U64 bishop_magic[64];
extern U64 rook_attacks[64][4096];
extern U64 bishop_attacks[64][512];
extern U64 pawn_attacks[2][64];

// The bit-packing macro
inline Move encode_move(int from, int to, int flag)
{
    return (flag << 12) | (to << 6) | from;
}
enum MoveFlag
{
    QUIET = 0,
    DOUBLE_PUSH = 1,
    K_CASTLE = 2,
    Q_CASTLE = 3,
    CAPTURE = 4,
    EP_CAPTURE = 5,

    N_PROMO = 8,
    B_PROMO = 9,
    R_PROMO = 10,
    Q_PROMO = 11,
    N_PROMO_CAP = 12,
    B_PROMO_CAP = 13,
    R_PROMO_CAP = 14,
    Q_PROMO_CAP = 15
};
inline U64 get_bishop_attacks(int square, U64 occupancy)
{
    occupancy &= bishop_masks[square];
    occupancy *= bishop_magic[square];
    occupancy >>= 64 - __builtin_popcountll(bishop_masks[square]);
    return bishop_attacks[square][occupancy];
}

inline U64 get_rook_attacks(int square, U64 occupancy)
{
    occupancy &= rook_masks[square];
    occupancy *= rook_magic[square];
    occupancy >>= 64 - __builtin_popcountll(rook_masks[square]);
    return rook_attacks[square][occupancy];
}

inline U64 get_queen_attacks(int square, U64 occupancy)
{
    return get_bishop_attacks(square, occupancy) | get_rook_attacks(square, occupancy);
}
void init_leapers();
void init_sliders();

void generate_moves(Position &pos, std::vector<Move> &move_list);
bool is_square_attacked(int square, int attacking_side, Position &pos);