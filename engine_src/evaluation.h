#pragma once
#include "position.h"
#include "nen_profiles.h"

void init_evaluation();
int evaluate(const Position &pos);

// Nen profile control
void set_nen_profile(const NenProfile &prof);
NenProfile get_nen_profile();

// King attack evaluation
int evaluate_king_attack(const Position &pos, int attacker_color);

// Exposed for incremental updates in make_move/unmake_move
int get_mg_value(int color, int piece, int sq);
int get_eg_value(int color, int piece, int sq);
int get_phase_weight(int piece);
