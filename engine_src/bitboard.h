#ifndef BITBOARD_H
#define BITBOARD_H

#include <cstdint>

using U64 = uint64_t;

inline void set_bit(U64 &bitboard, int square)
{
    U64 mask = 1ULL << square;
    bitboard |= mask;
}

inline void clear_bit(U64 &bitboard, int square)
{
    U64 mask = 1ULL << square;
    bitboard &= ~mask;
}
inline bool get_bit(U64 bitboard, int square)
{
    U64 mask = 1ULL << square;
    if (bitboard & mask)
    {
        return true;
    }
    else
    {
        return false;
    }
}
inline int pop_lsb(U64 &board)
{
    int square = __builtin_ctzll(board);
    board &= board - 1;
    return square;
}
void print_bitboard(U64 board);
#endif