#pragma once
#include "position.h"
#include <cstdint>
#include <cstring>

constexpr int NNUE_INPUTS = 768;
constexpr int NNUE_ACCUMULATOR_SIZE = 256;
constexpr int NNUE_HIDDEN1_SIZE = 32;
constexpr int NNUE_SCALE = 16;

struct Accumulator
{
    alignas(64) int16_t values[2][NNUE_ACCUMULATOR_SIZE];
};

inline int get_feature_index(int piece_color, int piece_type, int sq, int perspective)
{
    int piece_index = (piece_color == perspective) ? piece_type : (piece_type + 6);
    int square = (perspective == WHITE) ? sq : (sq ^ 56);
    return piece_index * 64 + square;
}

void init_nnue();
bool load_nnue_file(const char *filename);
void refresh_accumulator(const Position &pos, Accumulator &acc);
int evaluate_nnue(const Position &pos, const Accumulator &acc);
int evaluate_nnue_position(const Position &pos);
bool is_nnue_loaded();
void set_use_nnue(bool enable);
bool get_use_nnue();
