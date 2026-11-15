#pragma once
#include "position.h"
#include "movegen.h"
#include <cstdint>

const char *move_to_uci(Move m);
uint64_t perft(Position &pos, int depth);
void perft_divide(Position &pos, int depth);
void run_perft_suite(int max_depth = 4);
uint64_t bench_perft(Position &pos, int depth);
