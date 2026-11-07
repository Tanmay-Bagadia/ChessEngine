#include "bitboard.h"
#include "movegen.h"
#include "zobrist.h"
#include "evaluation.h"
#include "search.h"
#include "uci.h"

int main()
{
    init_leapers();
    init_zobrist();
    init_sliders();
    init_evaluation();
    init_tt(16);

    uci_loop();

    return 0;
}
