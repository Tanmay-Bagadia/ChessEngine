#pragma once
#include "position.h"
#include <cstdint>
#include <cstring>

constexpr int NNUE_INPUTS = 768;
constexpr int NNUE_ACCUMULATOR_SIZE = 256;
constexpr int NNUE_QA = 181; // 181^2 = 32761 fits exactly in int16_t for NEON SCReLU
constexpr int NNUE_QB = 64;
constexpr int NNUE_OUTPUT_SCALE = 400;

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

// Incremental accumulator helpers (called from position.cpp when NNUE is active)
void nnue_reset_accumulator(int16_t acc[2][NNUE_ACCUMULATOR_SIZE]);
void nnue_add_feature(int16_t acc[2][NNUE_ACCUMULATOR_SIZE], int piece_color, int piece_type, int sq);
void nnue_sub_feature(int16_t acc[2][NNUE_ACCUMULATOR_SIZE], int piece_color, int piece_type, int sq);
void nnue_move_feature(int16_t acc[2][NNUE_ACCUMULATOR_SIZE], int piece_color, int piece_type, int from_sq, int to_sq);

void refresh_accumulator(Position &pos);
void refresh_accumulator(const Position &pos, Accumulator &acc);
int evaluate_nnue(const Position &pos, const Accumulator &acc);
int evaluate_nnue_position(const Position &pos);
bool is_nnue_loaded();
void set_use_nnue(bool enable);
bool get_use_nnue();
