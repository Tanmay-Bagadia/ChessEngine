#include "nnue.h"
#include "bitboard.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#define USE_NEON_SIMD 1
#endif

static bool nnue_loaded = false;
static bool use_nnue = false;

alignas(64) static int16_t feature_weights[NNUE_INPUTS][NNUE_ACCUMULATOR_SIZE];
alignas(64) static int16_t feature_biases[NNUE_ACCUMULATOR_SIZE];
alignas(64) static int16_t output_weights[NNUE_ACCUMULATOR_SIZE * 2];
static int32_t output_bias = 0;

constexpr uint32_t NNUE_MAGIC = 0x33764E4E; // "NNv3" — SCReLU (768->256)x2->1

void init_nnue()
{
    const char *candidates[] = {"nnue.bin", "../nnue.bin", "../../nnue.bin"};
    for (const char *path : candidates)
    {
        if (load_nnue_file(path))
        {
            use_nnue = false; // Default to classical PeSTO until toggled on
            return;
        }
    }
    nnue_loaded = false;
    use_nnue = false;
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
        std::cerr << "Invalid NNUE file format (expected NNv3): " << filename << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(feature_weights), sizeof(feature_weights));
    file.read(reinterpret_cast<char*>(feature_biases), sizeof(feature_biases));
    file.read(reinterpret_cast<char*>(output_weights), sizeof(output_weights));
    file.read(reinterpret_cast<char*>(&output_bias), sizeof(output_bias));

    if (!file.good())
    {
        std::cerr << "Failed to read complete NNUE v3 model weights" << std::endl;
        return false;
    }

    nnue_loaded = true;
    use_nnue = true;
    std::cout << "info string Loaded NNUE net from " << filename << std::endl;
    return true;
}

void nnue_reset_accumulator(int16_t acc[2][NNUE_ACCUMULATOR_SIZE])
{
    std::memcpy(acc[WHITE], feature_biases, sizeof(feature_biases));
    std::memcpy(acc[BLACK], feature_biases, sizeof(feature_biases));
}

void nnue_add_feature(int16_t acc[2][NNUE_ACCUMULATOR_SIZE], int piece_color, int piece_type, int sq)
{
    const int wf = get_feature_index(piece_color, piece_type, sq, WHITE);
    const int bf = get_feature_index(piece_color, piece_type, sq, BLACK);
    const int16_t *ww = feature_weights[wf];
    const int16_t *bw = feature_weights[bf];

#ifdef USE_NEON_SIMD
    for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i += 8)
    {
        vst1q_s16(&acc[WHITE][i], vaddq_s16(vld1q_s16(&acc[WHITE][i]), vld1q_s16(&ww[i])));
        vst1q_s16(&acc[BLACK][i], vaddq_s16(vld1q_s16(&acc[BLACK][i]), vld1q_s16(&bw[i])));
    }
#else
    for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i++)
    {
        acc[WHITE][i] += ww[i];
        acc[BLACK][i] += bw[i];
    }
#endif
}

void nnue_sub_feature(int16_t acc[2][NNUE_ACCUMULATOR_SIZE], int piece_color, int piece_type, int sq)
{
    const int wf = get_feature_index(piece_color, piece_type, sq, WHITE);
    const int bf = get_feature_index(piece_color, piece_type, sq, BLACK);
    const int16_t *ww = feature_weights[wf];
    const int16_t *bw = feature_weights[bf];

#ifdef USE_NEON_SIMD
    for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i += 8)
    {
        vst1q_s16(&acc[WHITE][i], vsubq_s16(vld1q_s16(&acc[WHITE][i]), vld1q_s16(&ww[i])));
        vst1q_s16(&acc[BLACK][i], vsubq_s16(vld1q_s16(&acc[BLACK][i]), vld1q_s16(&bw[i])));
    }
#else
    for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i++)
    {
        acc[WHITE][i] -= ww[i];
        acc[BLACK][i] -= bw[i];
    }
#endif
}

void nnue_move_feature(int16_t acc[2][NNUE_ACCUMULATOR_SIZE], int piece_color, int piece_type, int from_sq, int to_sq)
{
    const int wf_from = get_feature_index(piece_color, piece_type, from_sq, WHITE);
    const int wf_to   = get_feature_index(piece_color, piece_type, to_sq, WHITE);
    const int bf_from = get_feature_index(piece_color, piece_type, from_sq, BLACK);
    const int bf_to   = get_feature_index(piece_color, piece_type, to_sq, BLACK);

    const int16_t *ww_from = feature_weights[wf_from];
    const int16_t *ww_to   = feature_weights[wf_to];
    const int16_t *bw_from = feature_weights[bf_from];
    const int16_t *bw_to   = feature_weights[bf_to];

#ifdef USE_NEON_SIMD
    for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i += 8)
    {
        int16x8_t aw = vld1q_s16(&acc[WHITE][i]);
        aw = vaddq_s16(vsubq_s16(aw, vld1q_s16(&ww_from[i])), vld1q_s16(&ww_to[i]));
        vst1q_s16(&acc[WHITE][i], aw);

        int16x8_t ab = vld1q_s16(&acc[BLACK][i]);
        ab = vaddq_s16(vsubq_s16(ab, vld1q_s16(&bw_from[i])), vld1q_s16(&bw_to[i]));
        vst1q_s16(&acc[BLACK][i], ab);
    }
#else
    for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i++)
    {
        acc[WHITE][i] += ww_to[i] - ww_from[i];
        acc[BLACK][i] += bw_to[i] - bw_from[i];
    }
#endif
}

static void compute_accumulator_raw(const Position &pos, int16_t acc[2][NNUE_ACCUMULATOR_SIZE])
{
    nnue_reset_accumulator(acc);

    for (int c = 0; c < 2; c++)
    {
        for (int p = 0; p < 6; p++)
        {
            U64 bb = pos.pieces[c][p];
            while (bb)
            {
                int sq = pop_lsb(bb);
                nnue_add_feature(acc, c, p, sq);
            }
        }
    }
}

void refresh_accumulator(Position &pos)
{
    if (!nnue_loaded)
    {
        pos.nnue_acc_valid = false;
        return;
    }
    compute_accumulator_raw(pos, pos.nnue_acc);
    pos.nnue_acc_valid = true;
}

void refresh_accumulator(const Position &pos, Accumulator &acc)
{
    compute_accumulator_raw(pos, acc.values);
}

static inline int evaluate_nnue_raw(const int16_t acc[2][NNUE_ACCUMULATOR_SIZE], int side_to_move)
{
    const int16_t *acc_us   = acc[side_to_move];
    const int16_t *acc_them = acc[side_to_move ^ 1];
    const int16_t *w_us     = &output_weights[0];
    const int16_t *w_them   = &output_weights[NNUE_ACCUMULATOR_SIZE];

#ifdef USE_NEON_SIMD
    const int16x8_t zero = vdupq_n_s16(0);
    const int16x8_t qa   = vdupq_n_s16(NNUE_QA);
    int32x4_t sum0 = vdupq_n_s32(0);
    int32x4_t sum1 = vdupq_n_s32(0);

    for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i += 8)
    {
        int16x8_t v1 = vminq_s16(vmaxq_s16(vld1q_s16(&acc_us[i]), zero), qa);
        int16x8_t sq1 = vmulq_s16(v1, v1);
        int16x8_t w1 = vld1q_s16(&w_us[i]);
        sum0 = vmlal_s16(sum0, vget_low_s16(sq1), vget_low_s16(w1));
        sum1 = vmlal_s16(sum1, vget_high_s16(sq1), vget_high_s16(w1));

        int16x8_t v2 = vminq_s16(vmaxq_s16(vld1q_s16(&acc_them[i]), zero), qa);
        int16x8_t sq2 = vmulq_s16(v2, v2);
        int16x8_t w2 = vld1q_s16(&w_them[i]);
        sum0 = vmlal_s16(sum0, vget_low_s16(sq2), vget_low_s16(w2));
        sum1 = vmlal_s16(sum1, vget_high_s16(sq2), vget_high_s16(w2));
    }

    int32_t l0[4], l1[4];
    vst1q_s32(l0, sum0);
    vst1q_s32(l1, sum1);
    int64_t sum = (int64_t)output_bias
                + l0[0] + l0[1] + l0[2] + l0[3]
                + l1[0] + l1[1] + l1[2] + l1[3];
#else
    int64_t sum = output_bias;
    for (int i = 0; i < NNUE_ACCUMULATOR_SIZE; i++)
    {
        int32_t v1 = std::clamp((int)acc_us[i], 0, NNUE_QA);
        sum += (int64_t)(v1 * v1) * (int64_t)w_us[i];

        int32_t v2 = std::clamp((int)acc_them[i], 0, NNUE_QA);
        sum += (int64_t)(v2 * v2) * (int64_t)w_them[i];
    }
#endif

    constexpr int64_t DIVISOR = (int64_t)NNUE_QA * NNUE_QA * NNUE_QB;
    return (int)((sum * NNUE_OUTPUT_SCALE) / DIVISOR);
}

int evaluate_nnue(const Position &pos, const Accumulator &acc)
{
    return evaluate_nnue_raw(acc.values, pos.side_to_move);
}

int evaluate_nnue_position(const Position &pos)
{
    if (pos.nnue_acc_valid)
    {
        return evaluate_nnue_raw(pos.nnue_acc, pos.side_to_move);
    }
    Accumulator acc;
    refresh_accumulator(pos, acc);
    return evaluate_nnue_raw(acc.values, pos.side_to_move);
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
