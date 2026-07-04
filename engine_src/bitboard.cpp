#include <iostream>
#include "bitboard.h"

void print_bitboard(U64 board)
{
    std::cout << "\n";

    for (int rank = 7; rank >= 0; rank--)
    {
        for (int file = 0; file < 8; file++)
        {

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