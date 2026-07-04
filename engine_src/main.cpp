#include <iostream>
#include "bitboard.h"
#include "movegen.h"

int main()
{
    std::cout << "Booting Engine Architecture...\n";

    init_leapers();
    init_sliders();

    std::cout << "Initialization Complete. All O(1) Lookup Tables Built.\n";
    std::cout << "========================================================\n\n";

    int test_square = 28; // Square e4

    std::cout << "--- TESTING ROOK MASK FOR e4 ---";
    print_bitboard(rook_masks[test_square]);

    std::cout << "--- TESTING BISHOP MASK FOR e4 ---";
    print_bitboard(bishop_masks[test_square]);

    return 0;
}