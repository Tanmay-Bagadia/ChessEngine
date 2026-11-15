#pragma once
#include "position.h"
#include "movegen.h"
#include <cstdint>
#include <cstddef>
#include <atomic>

constexpr int MAX_PLY = 128;
constexpr int MATE_SCORE = 49000;
constexpr int INF_SCORE = 50000;

enum TTFlag : uint8_t {
    TT_NONE = 0,
    TT_EXACT = 1,
    TT_ALPHA = 2,
    TT_BETA = 3
};

struct TTEntry {
    U64 hash;
    int32_t score;
    Move best_move;
    int8_t depth;
    uint8_t flag;
    uint8_t age; // For replacement scheme
};

void init_tt(size_t mb = 16);
void clear_tt();
void age_tt();

struct SearchInfo {
    int depth;
    uint64_t nodes;
    Move best_move;
    int score;
    std::atomic<bool> stopped;
    int64_t time_limit_ms;
    int64_t start_time_ms;

    int multipv;
    Move killers[MAX_PLY + 1][2];
    int history[2][64][64];
    Move counter_moves[2][64][64]; // counter move table

    Move pv[MAX_PLY + 1][MAX_PLY + 1];
    int pv_length[MAX_PLY + 1];

    U64 rep_history[4096];
    int rep_count;
    int game_ply;

    uint8_t tt_age;

    // Move ordering state for previous move (for counter move heuristic)
    Move prev_move;

    SearchInfo() : depth(0), nodes(0), best_move(0), score(0),
                   stopped(false), time_limit_ms(0), start_time_ms(0),
                   multipv(1), rep_count(0), game_ply(0), tt_age(0), prev_move(0) {
        clear();
    }

    void clear() {
        nodes = 0;
        best_move = 0;
        score = 0;
        rep_count = game_ply;
        tt_age++;
        for (int i = 0; i < MAX_PLY + 1; i++) {
            killers[i][0] = killers[i][1] = 0;
            pv_length[i] = 0;
            for (int j = 0; j < MAX_PLY + 1; j++)
                pv[i][j] = 0;
        }
        for (int c = 0; c < 2; c++)
            for (int f = 0; f < 64; f++)
                for (int t = 0; t < 64; t++) {
                    history[c][f][t] = 0;
                    counter_moves[c][f][t] = 0;
                }
    }
};

int64_t get_time_ms();
void search_position(Position &pos, SearchInfo &info);
