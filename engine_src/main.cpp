#include <iostream>
#include "bitboard.h"
#include "position.h"
#include "movegen.h"
#include "perft.h"

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
    load_fen(pos, START_FEN);
    print_board(pos);

    MoveList move_list;
    generate_moves(pos, move_list);
    std::cout << "\nTotal moves: " << move_list.size() << " (Expected: 20)\n";

    // PERFT DIVIDE TEST (Depth 1)
    std::cout << "\n--- PERFT DIVIDE TEST (Starting Position, Depth 1) ---\n";
    perft_divide(pos, 1);

    return 0;
}