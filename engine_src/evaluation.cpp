#include "evaluation.h"
#include "bitboard.h"
#include "movegen.h"
#include "nnue.h"
#include <algorithm>
#include <cstdlib>

// ===== PeSTO piece-square tables (rank 8 at top, rank 1 at bottom) =====

static const int mg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     98, 134,  61,  95,  68, 126,  34, -11,
     -6,   7,  26,  31,  65,  56,  25, -20,
    -14,  13,   6,  21,  23,  12,  17, -23,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -35,  -1, -20, -23, -15,  24,  38, -22,
      0,   0,   0,   0,   0,   0,   0,   0,
};

static const int mg_knight_table[64] = {
   -167, -89, -34, -49,  61, -97, -15,-107,
    -73, -41,  72,  36,  23,  62,   7, -17,
    -47,  60,  37,  65,  84, 129,  73,  44,
     -9,  17,  19,  53,  37,  69,  18,  22,
    -13,   4,  16,  13,  28,  19,  21,  -8,
    -23,  -9,  12,  10,  19,  17,  25, -16,
    -29, -53, -12,  -3,  -1,  18, -14, -19,
   -105, -21, -58, -33, -17, -28, -19, -23,
};

static const int mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

static const int mg_rook_table[64] = {
     32,  42,  32,  51,  63,   9,  31,  43,
     27,  32,  58,  62,  80,  67,  26,  44,
     -5,  19,  26,  36,  17,  45,  61,  16,
    -24, -11,   7,  26,  24,  35,  -8, -20,
    -36, -26, -12,  -1,   9,  -7,   6, -23,
    -45, -25, -16, -17,   3,   0,  -5, -33,
    -44, -16, -20,  -9,  -1,  11,  -6, -71,
    -19, -13,   1,  17,  16,   7, -37, -26,
};

static const int mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};

static const int mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

static const int eg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8, -10,  -6,  -8,  -4, -14,
      0,   0,   0,   0,   0,   0,   0,   0,
};

static const int eg_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

static const int eg_bishop_table[64] = {
    -14, -21, -11,  -8,  -7,  -9, -17, -24,
     -8,  -4,   7, -12,  -3, -13,  -4, -14,
      2,  -8,   0,  -1,  -2,   6,   0,   4,
     -3,   9,  12,   9,  14,  10,   3,   2,
     -6,   3,  13,  19,   7,  10,  -3,  -9,
    -12,  -3,   8,  10,  13,   3,  -7, -15,
    -14, -18,  -7,  -1,   4,  -9, -15, -27,
    -23,  -9, -23,  -5,  -9, -16,  -5, -17,
};

static const int eg_rook_table[64] = {
     13,  10,  18,  15,  12,  12,   8,   5,
     11,  13,  13,  11,  -3,   3,   8,   3,
      7,   7,   7,   5,   4,  -3,  -5,  -3,
      4,   3,  13,   1,   2,   1,  -1,   2,
      3,   5,   8,   4,  -5,  -6,  -8, -11,
     -4,   0,  -5,  -1,  -7, -12,  -8, -16,
     -6,  -6,   0,   2,  -9,  -9, -11,  -3,
     -9,   2,   3,  -1,  -5, -13,   4, -20,
};

static const int eg_queen_table[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

static const int eg_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43,
};

static const int* mg_tables[6] = {
    mg_pawn_table, mg_knight_table, mg_bishop_table,
    mg_rook_table, mg_queen_table, mg_king_table
};

static const int* eg_tables[6] = {
    eg_pawn_table, eg_knight_table, eg_bishop_table,
    eg_rook_table, eg_queen_table, eg_king_table
};

// Material values (midgame / endgame)
static const int mg_material[6] = { 82, 337, 365, 477, 1025, 0 };
static const int eg_material[6] = { 94, 281, 297, 512,  936, 0 };

// Phase weights for tapered eval
static const int phase_weights[6] = { 0, 1, 1, 2, 4, 0 };
static const int TOTAL_PHASE = 24;

// Precomputed tables for fast access
static int mg_table[2][6][64]; // mg_table[color][piece][sq]
static int eg_table[2][6][64]; // eg_table[color][piece][sq]
static bool tables_initialized = false;

// ===== Positional evaluation bonuses =====
// (These are computed dynamically, not incremental)

static constexpr int BISHOP_PAIR_MG = 30;
static constexpr int BISHOP_PAIR_EG = 50;

static constexpr int DOUBLED_PAWN_MG = -5;
static constexpr int DOUBLED_PAWN_EG = -15;

static constexpr int ISOLATED_PAWN_MG = -10;
static constexpr int ISOLATED_PAWN_EG = -15;

static constexpr int PASSED_PAWN_BONUS[8] = { 0, 5, 10, 20, 40, 70, 110, 0 };

static constexpr int ROOK_OPEN_FILE_MG = 20;
static constexpr int ROOK_SEMI_OPEN_FILE_MG = 10;

static constexpr int KNIGHT_MOBILITY_MG = 4;
static constexpr int BISHOP_MOBILITY_MG = 5;
static constexpr int ROOK_MOBILITY_MG = 3;
static constexpr int QUEEN_MOBILITY_MG = 2;

static constexpr int KING_SHIELD_BONUS = 10;

// ===== Public interface for incremental updates =====

int get_mg_value(int color, int piece, int sq) {
    int table_sq = (color == WHITE) ? (sq ^ 56) : sq;
    return mg_material[piece] + mg_tables[piece][table_sq];
}

int get_eg_value(int color, int piece, int sq) {
    int table_sq = (color == WHITE) ? (sq ^ 56) : sq;
    return eg_material[piece] + eg_tables[piece][table_sq];
}

int get_phase_weight(int piece) {
    return phase_weights[piece];
}

// ===== Initialization =====

void init_evaluation() {
    if (tables_initialized) return;

    for (int piece = 0; piece < 6; piece++) {
        for (int sq = 0; sq < 64; sq++) {
            int white_sq = sq ^ 56; // flip rank for white
            int black_sq = sq;

            mg_table[WHITE][piece][sq] = mg_material[piece] + mg_tables[piece][white_sq];
            eg_table[WHITE][piece][sq] = eg_material[piece] + eg_tables[piece][white_sq];

            mg_table[BLACK][piece][sq] = mg_material[piece] + mg_tables[piece][black_sq];
            eg_table[BLACK][piece][sq] = eg_material[piece] + eg_tables[piece][black_sq];
        }
    }
    tables_initialized = true;
}

// ===== Positional evaluation terms =====

static int evaluate_pawn_structure(const Position &pos, int eg_bonus[2]) {
    int mg = 0;
    eg_bonus[0] = eg_bonus[1] = 0;

    for (int color = 0; color < 2; color++) {
        U64 pawns = pos.pieces[color][PAWN];
        U64 enemy_pawns = pos.pieces[color ^ 1][PAWN];

        U64 bb = pawns;
        while (bb) {
            int sq = pop_lsb(bb);
            int file = sq % 8;
            int rank = sq / 8;

            // Doubled pawns: another friendly pawn on same file
            U64 file_pawns = pawns & FILE_MASKS[file];
            if (popcount(file_pawns) > 1) {
                mg += DOUBLED_PAWN_MG;
                eg_bonus[color] += DOUBLED_PAWN_EG;
            }

            // Isolated pawns: no friendly pawns on adjacent files
            U64 adj_pawns = pawns & ADJACENT_FILES[file];
            if (adj_pawns == 0) {
                mg += ISOLATED_PAWN_MG;
                eg_bonus[color] += ISOLATED_PAWN_EG;
            }

            // Passed pawns: no enemy pawns on same or adjacent files ahead
            int forward_rank = (color == WHITE) ? rank + 1 : rank - 1;
            bool passed = true;
            U64 adj_files = FILE_MASKS[file] | ADJACENT_FILES[file];
            U64 ahead_mask = 0;
            if (color == WHITE) {
                for (int r = forward_rank; r <= 7; r++)
                    ahead_mask |= RANK_MASKS[r];
            } else {
                for (int r = forward_rank; r >= 0; r--)
                    ahead_mask |= RANK_MASKS[r];
            }
            if (enemy_pawns & adj_files & ahead_mask)
                passed = false;

            if (passed) {
                int rel_rank = (color == WHITE) ? rank : (7 - rank);
                eg_bonus[color] += PASSED_PAWN_BONUS[rel_rank];
            }
        }
    }

    return mg;
}

static int evaluate_mobility(const Position &pos) {
    int mg = 0;

    // Knight mobility
    for (int color = 0; color < 2; color++) {
        int sign = (color == WHITE) ? 1 : -1;
        U64 knights = pos.pieces[color][KNIGHT];
        while (knights) {
            int sq = pop_lsb(knights);
            U64 attacks = knight_attacks[sq] & ~pos.occupancy[color];
            int mob = popcount(attacks);
            mg += sign * (mob * KNIGHT_MOBILITY_MG - 4 * KNIGHT_MOBILITY_MG);
        }

        // Bishop mobility
        U64 bishops = pos.pieces[color][BISHOP];
        while (bishops) {
            int sq = pop_lsb(bishops);
            U64 attacks = get_bishop_attacks(sq, pos.occupancy[BOTH]) & ~pos.occupancy[color];
            int mob = popcount(attacks);
            mg += sign * (mob * BISHOP_MOBILITY_MG - 7 * BISHOP_MOBILITY_MG);
        }

        // Rook mobility
        U64 rooks = pos.pieces[color][ROOK];
        while (rooks) {
            int sq = pop_lsb(rooks);
            U64 attacks = get_rook_attacks(sq, pos.occupancy[BOTH]) & ~pos.occupancy[color];
            int mob = popcount(attacks);
            mg += sign * (mob * ROOK_MOBILITY_MG - 7 * ROOK_MOBILITY_MG);
        }

        // Queen mobility (lighter weight)
        U64 queens = pos.pieces[color][QUEEN];
        while (queens) {
            int sq = pop_lsb(queens);
            U64 attacks = get_queen_attacks(sq, pos.occupancy[BOTH]) & ~pos.occupancy[color];
            int mob = popcount(attacks);
            mg += sign * (mob * QUEEN_MOBILITY_MG - 14 * QUEEN_MOBILITY_MG);
        }
    }

    return mg;
}

static int evaluate_piece_bonuses(const Position &pos) {
    int mg = 0, eg = 0;

    for (int color = 0; color < 2; color++) {
        int sign = (color == WHITE) ? 1 : -1;

        // Bishop pair bonus
        if (popcount(pos.pieces[color][BISHOP]) >= 2) {
            mg += sign * BISHOP_PAIR_MG;
            eg += sign * BISHOP_PAIR_EG;
        }

        // Rook on open/semi-open file
        U64 rooks = pos.pieces[color][ROOK];
        while (rooks) {
            int sq = pop_lsb(rooks);
            int file = sq % 8;
            U64 file_mask = FILE_MASKS[file];
            bool friendly_pawns_on_file = (pos.pieces[color][PAWN] & file_mask) != 0;
            bool enemy_pawns_on_file = (pos.pieces[color ^ 1][PAWN] & file_mask) != 0;

            if (!friendly_pawns_on_file && !enemy_pawns_on_file)
                mg += sign * ROOK_OPEN_FILE_MG;
            else if (!friendly_pawns_on_file)
                mg += sign * ROOK_SEMI_OPEN_FILE_MG;
        }

        // King safety: pawn shield
        int king_sq = lsb(pos.pieces[color][KING]);
        int king_file = king_sq % 8;
        int king_rank = king_sq / 8;
        U64 shield_mask = 0;
        for (int f = std::max(0, king_file - 1); f <= std::min(7, king_file + 1); f++) {
            if (color == WHITE) {
                if (king_rank < 7) shield_mask |= (1ULL << ((king_rank + 1) * 8 + f));
                if (king_rank < 6) shield_mask |= (1ULL << ((king_rank + 2) * 8 + f));
            } else {
                if (king_rank > 0) shield_mask |= (1ULL << ((king_rank - 1) * 8 + f));
                if (king_rank > 1) shield_mask |= (1ULL << ((king_rank - 2) * 8 + f));
            }
        }
        int shield_count = popcount(pos.pieces[color][PAWN] & shield_mask);
        mg += sign * shield_count * KING_SHIELD_BONUS;
    }

    return mg + eg; // Simplified: both go into mg for now
}

// ===== Main evaluation =====

int evaluate(const Position &pos) {
    // Try NNUE first
    if (get_use_nnue() && is_nnue_loaded())
        return evaluate_nnue_position(pos);

    if (!tables_initialized)
        init_evaluation();

    // Use incremental material + PST scores
    int mg_white = pos.mg_score[WHITE];
    int eg_white = pos.eg_score[WHITE];
    int mg_black = pos.mg_score[BLACK];
    int eg_black = pos.eg_score[BLACK];
    int phase = std::min(pos.game_phase, TOTAL_PHASE);

    // Dynamic positional terms (not incremental)
    int eg_bonus[2] = {0, 0};
    int pawn_mg = evaluate_pawn_structure(pos, eg_bonus);
    int mobility_mg = evaluate_mobility(pos);
    int piece_mg = evaluate_piece_bonuses(pos);

    int mg = (mg_white - mg_black) + pawn_mg + mobility_mg + piece_mg;
    int eg = (eg_white - eg_black) + (eg_bonus[WHITE] - eg_bonus[BLACK]);

    // Tapered evaluation
    int mg_phase = phase;
    int eg_phase = TOTAL_PHASE - phase;
    int score = (mg * mg_phase + eg * eg_phase) / TOTAL_PHASE;

    // Return score from side-to-move perspective
    return (pos.side_to_move == WHITE) ? score : -score;
}
