#include <iostream>
#include "bitboard.h"
#include "movegen.h"

int main()
{
    std::cout << "Booting Engine Architecture...\n";

    init_leapers();
    init_sliders();

    int test_square = 28; // Square e4

    std::cout << "--- TESTING ROOK MASK FOR e4 ---";
    print_bitboard(rook_masks[test_square]);

    std::cout << "--- TESTING BISHOP MASK FOR e4 ---";
    print_bitboard(bishop_masks[test_square]);

    // MOVE GENERATION TEST
    std::cout << "\n--- MOVE GENERATION TEST (Starting Position) ---\n";
    Position pos;
    init_position(pos);
    print_board(pos);

    std::vector<Move> move_list;
    generate_moves(pos, move_list);
    std::cout << "\nTotal moves: " << move_list.size() << " (Expected: 20)\n";

    return 0;
}