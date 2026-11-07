#include "nnue.h"
#include "bitboard.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

static bool nnue_loaded = false;
static bool use_nnue = false;

alignas(64) static int16_t feature_weights[NNUE_INPUTS][NNUE_ACCUMULATOR_SIZE];
alignas(64) static int16_t feature_biases[NNUE_ACCUMULATOR_SIZE];
alignas(64) static int8_t  l1_weights[512][NNUE_HIDDEN1_SIZE];
alignas(64) static int32_t l1_biases[NNUE_HIDDEN1_SIZE];
alignas(64) static int8_t  l2_weights[NNUE_HIDDEN1_SIZE][NNUE_HIDDEN1_SIZE];
alignas(64) static int32_t l2_biases[NNUE_HIDDEN1_SIZE];
alignas(64) static int8_t  output_weights[NNUE_HIDDEN1_SIZE];
static int32_t output_bias = 0;

constexpr uint32_t NNUE_MAGIC = 0x31764E4E; // "NNv1"

void init_nnue()
{
    // Try to load default nnue file if present
    if (load_nnue_file("nnue.bin"))
    {
        use_nnue = true;
    }
    else
    {
        nnue_loaded = false;
        use_nnue = false;
    }
}

bool load_nnue_file(const char *filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
        return false;

    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != NNUE_MAGIC)
    {
        std::cerr << "Invalid NNUE file format: " << filename << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(feature_weights), sizeof(feature_weights));
    file.read(reinterpret_cast<char*>(feature_biases), sizeof(feature_biases));
    file.read(reinterpret_cast<char*>(l1_weights), sizeof(l1_weights));
    file.read(reinterpret_cast<char*>(l1_biases), sizeof(l1_biases));
    file.read(reinterpret_cast<char*>(l2_weights), sizeof(l2_weights));
    file.read(reinterpret_cast<char*>(l2_biases), sizeof(l2_biases));
    file.read(reinterpret_cast<char*>(output_weights), sizeof(output_weights));
    file.read(reinterpret_cast<char*>(&output_bias), sizeof(output_bias));

    if (!file.good())
    {
        std::cerr << "Failed to read complete NNUE model weights" << std::endl;
        return false;
    }

    nnue_loaded = true;
    use_nnue = true;
    std::cout << "info string Loaded NNUE net from " << filename << std::endl;
    return true;
}

void refresh_accumulator(const Position &pos, Accumulator &acc)
{
    for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i++)
    {
        acc.values[WHITE][i] = feature_biases[i];
        acc.values[BLACK][i] = feature_biases[i];
    }

    for (int c = 0; c < 2; c++)
    {
        for (int p = 0; p < 6; p++)
        {
            U64 bb = pos.pieces[c][p];
            while (bb)
            {
                int sq = pop_lsb(bb);
                int white_feature = get_feature_index(c, p, sq, WHITE);
                int black_feature = get_feature_index(c, p, sq, BLACK);

                for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i++)
                {
                    acc.values[WHITE][i] += feature_weights[white_feature][i];
                    acc.values[BLACK][i] += feature_weights[black_feature][i];
                }
            }
        }
    }
}

int evaluate_nnue(const Position &pos, const Accumulator &acc)
{
    int us = pos.side_to_move;
    int them = us ^ 1;

    const int16_t *acc_us = acc.values[us];
    const int16_t *acc_them = acc.values[them];

    alignas(64) uint8_t l1_input[512];
    for (int i = 0; i < 256; i++)
    {
        int16_t val = acc_us[i];
        l1_input[i] = (uint8_t)std::clamp((int)val, 0, 127);
    }
    for (int i = 0; i < 256; i++)
    {
        int16_t val = acc_them[i];
        l1_input[256 + i] = (uint8_t)std::clamp((int)val, 0, 127);
    }

    alignas(64) int32_t l1_out[NNUE_HIDDEN1_SIZE];
    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
    {
        int32_t sum = l1_biases[i];
        for (int j = 0; j < 512; j++)
            sum += (int32_t)l1_input[j] * (int32_t)l1_weights[j][i];
        l1_out[i] = sum;
    }

    alignas(64) uint8_t l2_input[NNUE_HIDDEN1_SIZE];
    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
    {
        int32_t val = l1_out[i] / 64;
        l2_input[i] = (uint8_t)std::clamp((int)val, 0, 127);
    }

    alignas(64) int32_t l2_out[NNUE_HIDDEN1_SIZE];
    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
    {
        int32_t sum = l2_biases[i];
        for (int j = 0; j < NNUE_HIDDEN1_SIZE; j++)
            sum += (int32_t)l2_input[j] * (int32_t)l2_weights[j][i];
        l2_out[i] = sum;
    }

    alignas(64) uint8_t l3_input[NNUE_HIDDEN1_SIZE];
    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
    {
        int32_t val = l2_out[i] / 64;
        l3_input[i] = (uint8_t)std::clamp((int)val, 0, 127);
    }

    int32_t output = output_bias;
    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++)
        output += (int32_t)l3_input[i] * (int32_t)output_weights[i];

    return output / NNUE_SCALE;
}

int evaluate_nnue_position(const Position &pos)
{
    Accumulator acc;
    refresh_accumulator(pos, acc);
    return evaluate_nnue(pos, acc);
}

bool is_nnue_loaded()
{
    return nnue_loaded;
}

void set_use_nnue(bool enable)
{
    use_nnue = enable && nnue_loaded;
}

bool get_use_nnue()
{
    return use_nnue;
}
