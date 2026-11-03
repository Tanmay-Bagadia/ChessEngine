#include <iostream>
#include "bitboard.h"
#include "position.h"
#include "movegen.h"
#include "perft.h"
#include "zobrist.h"

int main()
{
    std::cout << "Booting Engine Architecture...\n";

    init_leapers();
    init_zobrist();
    init_sliders();

    run_perft_suite();

    return 0;
}