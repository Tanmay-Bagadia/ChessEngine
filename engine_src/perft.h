#pragma once
#include "position.h"
#include "movegen.h"
#include <cstdint>

// Converts a Move (uint16_t) to standard UCI notation (e.g. "e2e4", "e7e8q")
const char *move_to_uci(Move m);

// Recursive perft counter (returns leaf node count at depth)
uint64_t perft(Position &pos, int depth);

// Perft Divide: prints each root move and its subtree leaf node count
void perft_divide(Position &pos, int depth);

// Perft suite test structure
struct PerftTest
{
    const char *name;
    const char *fen;
    int depth;
    uint64_t expected_nodes;
};

// Runs standard perft benchmark tests
void run_perft_suite(int max_depth = 4);

