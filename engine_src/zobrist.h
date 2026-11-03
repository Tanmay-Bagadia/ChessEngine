#pragma once
#include "bitboard.h"

struct Position;

extern U64 piece_keys[2][6][64];
extern U64 castle_keys[16];
extern U64 ep_keys[8];
extern U64 side_key;

void init_zobrist();

U64 compute_hash(const Position &pos);