#include <iostream>
#include "bitboard.h"

const U64 FILE_MASKS[8] = {
    0x0101010101010101ULL, // File A
    0x0202020202020202ULL, // File B
    0x0404040404040404ULL, // File C
    0x0808080808080808ULL, // File D
    0x1010101010101010ULL, // File E
    0x2020202020202020ULL, // File F
    0x4040404040404040ULL, // File G
    0x8080808080808080ULL  // File H
};

const U64 RANK_MASKS[8] = {
    0x00000000000000FFULL, // Rank 1
    0x000000000000FF00ULL, // Rank 2
    0x0000000000FF0000ULL, // Rank 3
    0x00000000FF000000ULL, // Rank 4
    0x000000FF00000000ULL, // Rank 5
    0x0000FF0000000000ULL, // Rank 6
    0x00FF000000000000ULL, // Rank 7
    0xFF00000000000000ULL  // Rank 8
};

const U64 ADJACENT_FILES[8] = {
    0x0202020202020202ULL,                     // File A: adjacent is B
    0x0101010101010101ULL | 0x0404040404040404ULL, // File B: A+C
    0x0202020202020202ULL | 0x0808080808080808ULL, // File C: B+D
    0x0404040404040404ULL | 0x1010101010101010ULL, // File D: C+E
    0x0808080808080808ULL | 0x2020202020202020ULL, // File E: D+F
    0x1010101010101010ULL | 0x4040404040404040ULL, // File F: E+G
    0x2020202020202020ULL | 0x8080808080808080ULL, // File G: F+H
    0x4040404040404040ULL                      // File H: adjacent is G
};

void print_bitboard(U64 board) {
    std::cout << "\n";
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            if (file == 0)
                std::cout << rank + 1 << "  ";
            if (board & (1ULL << square))
                std::cout << "1 ";
            else
                std::cout << ". ";
        }
        std::cout << "\n";
    }
    std::cout << "\n   a b c d e f g h\n\n";
}
