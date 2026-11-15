#pragma once
#include "position.h"

void init_evaluation();
int evaluate(const Position &pos);

// Exposed for incremental updates in make_move/unmake_move
int get_mg_value(int color, int piece, int sq);
int get_eg_value(int color, int piece, int sq);
int get_phase_weight(int piece);
