#ifndef BITBOARD_H
#define BITBOARD_H

#include <cstdint>
#include <cstdio>

using U64 = uint64_t;

// Square enumeration for readability
enum Square {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_NONE = 64
};

inline constexpr int popcount(U64 bb) {
    return __builtin_popcountll(bb);
}

inline void set_bit(U64 &bitboard, int square) {
    bitboard |= 1ULL << square;
}

inline void clear_bit(U64 &bitboard, int square) {
    bitboard &= ~(1ULL << square);
}

inline bool get_bit(U64 bitboard, int square) {
    return (bitboard >> square) & 1ULL;
}

inline int pop_lsb(U64 &board) {
    int square = __builtin_ctzll(board);
    board &= board - 1;
    return square;
}

inline int lsb(U64 board) {
    return __builtin_ctzll(board);
}

inline U64 lsb_bit(U64 board) {
    return board & -board;
}

void print_bitboard(U64 board);

// File and rank masks
extern const U64 FILE_MASKS[8];
extern const U64 RANK_MASKS[8];
extern const U64 ADJACENT_FILES[8];

#endif
