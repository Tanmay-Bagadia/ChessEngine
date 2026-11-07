#pragma once
#include "position.h"
#include "movegen.h"
#include <cstdint>
#include <cstddef>
#include <atomic>

constexpr int MAX_PLY = 64;
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
};

void init_tt(size_t mb = 16);
void clear_tt();

struct SearchInfo {
    int depth;
    uint64_t nodes;
    Move best_move;
    int score;
    std::atomic<bool> stopped;
    int64_t time_limit_ms;
    int64_t start_time_ms;

    Move killers[MAX_PLY][2];
    int history[2][64][64];
    Move pv[MAX_PLY][MAX_PLY];
    int pv_length[MAX_PLY];

    U64 rep_history[2048];
    int rep_count;
    int game_ply;

    SearchInfo() : depth(0), nodes(0), best_move(0), score(0),
                   stopped(false), time_limit_ms(0), start_time_ms(0),
                   rep_count(0), game_ply(0) {
        clear();
    }

    void clear() {
        nodes = 0;
        best_move = 0;
        score = 0;
        rep_count = game_ply;
        for (int i = 0; i < MAX_PLY; i++) {
            killers[i][0] = killers[i][1] = 0;
            pv_length[i] = 0;
            for (int j = 0; j < MAX_PLY; j++)
                pv[i][j] = 0;
        }
        for (int c = 0; c < 2; c++)
            for (int f = 0; f < 64; f++)
                for (int t = 0; t < 64; t++)
                    history[c][f][t] = 0;
    }
};

int64_t get_time_ms();
void search_position(Position &pos, SearchInfo &info);
