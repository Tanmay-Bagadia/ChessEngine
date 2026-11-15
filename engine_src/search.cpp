#include "search.h"
#include "evaluation.h"
#include "movegen.h"
#include "perft.h"
#include "zobrist.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cmath>
#include <vector>

// ===== Transposition Table =====

static std::vector<TTEntry> tt_table;
static size_t tt_mask = 0;

void init_tt(size_t mb) {
    size_t bytes = mb * 1024 * 1024;
    size_t count = bytes / sizeof(TTEntry);
    size_t size = 1;
    while ((size << 1) <= count) size <<= 1;
    tt_table.resize(size);
    tt_mask = size - 1;
    clear_tt();
}

void clear_tt() {
    std::memset(tt_table.data(), 0, tt_table.size() * sizeof(TTEntry));
}

void age_tt() {
    // We don't clear entries but use age comparison in replacement
}

static inline TTEntry* tt_probe(U64 hash) {
    if (tt_table.empty()) return nullptr;
    return &tt_table[hash & tt_mask];
}

static inline void tt_store(U64 hash, Move best_move, int score, int depth, TTFlag flag, int ply, uint8_t age) {
    if (tt_table.empty()) return;
    TTEntry &entry = tt_table[hash & tt_mask];

    // Adjust mate scores relative to root
    if (score > MATE_SCORE - MAX_PLY) score += ply;
    else if (score < -MATE_SCORE + MAX_PLY) score -= ply;

    bool replace = false;
    if (entry.hash != hash) {
        replace = true;
    } else if (entry.age != age) {
        replace = true; // Old entry from previous search
    } else if (depth >= entry.depth) {
        replace = true;
    } else if (flag == TT_EXACT) {
        replace = true;
    }

    if (replace) {
        entry.hash = hash;
        entry.best_move = best_move ? best_move : ((entry.hash == hash) ? entry.best_move : 0);
        entry.score = (int32_t)score;
        entry.depth = (int8_t)depth;
        entry.flag = flag;
        entry.age = age;
    } else if (best_move) {
        entry.best_move = best_move;
    }
}

// ===== Time management =====

int64_t get_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ===== Move ordering =====

static const int MVV_LVA[6][6] = {
    { 105, 104, 103, 102, 101, 100 },
    { 205, 204, 203, 202, 201, 200 },
    { 305, 304, 303, 302, 301, 200 },
    { 405, 404, 403, 402, 401, 400 },
    { 505, 504, 503, 502, 501, 500 },
    { 605, 604, 603, 602, 601, 600 },
};

static int score_move(Move m, const Position &pos, const SearchInfo &info, int ply, Move hash_move) {
    if (m == hash_move) return 100000;

    int flag = get_flag(m);
    int from = get_from(m);
    int to = get_to(m);
    int side = pos.side_to_move;

    // Captures: MVV-LVA
    if (flag & 4) {
        int attacker = find_piece_at(pos, side, from);
        int victim = (flag == EP_CAPTURE) ? PAWN : find_piece_at(pos, side ^ 1, to);
        if (attacker >= 0 && victim >= 0)
            return MVV_LVA[victim][attacker] + 50000;
        return 50000;
    }

    // Promotions
    if (flag >= 8)
        return 45000 + (flag & 3);

    // Killer moves
    if (ply < MAX_PLY) {
        if (m == info.killers[ply][0]) return 40000;
        if (m == info.killers[ply][1]) return 39000;
    }

    // Counter move heuristic
    if (info.prev_move != 0) {
        int prev_to = get_to(info.prev_move);
        int prev_side = side ^ 1; // The side that just moved
        if (info.counter_moves[prev_side][get_from(info.prev_move)][prev_to] == m)
            return 38000;
    }

    // History heuristic (clamped)
    return std::min(info.history[side][from][to], 30000);
}

// Insertion sort: efficient for nearly-sorted arrays (which move lists tend to be)
static void sort_moves(MoveList &ml, const Position &pos, const SearchInfo &info, int ply, Move hash_move) {
    int scores[256];
    for (int i = 0; i < ml.size(); i++)
        scores[i] = score_move(ml[i], pos, info, ply, hash_move);

    // Insertion sort
    for (int i = 1; i < ml.size(); i++) {
        Move key_move = ml[i];
        int key_score = scores[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < key_score) {
            ml[j + 1] = ml[j];
            scores[j + 1] = scores[j];
            j--;
        }
        ml[j + 1] = key_move;
        scores[j + 1] = key_score;
    }
}

// ===== Utility =====

static inline void check_time(SearchInfo &info) {
    if (info.time_limit_ms > 0 && (info.nodes & 2047) == 0) {
        if (get_time_ms() - info.start_time_ms >= info.time_limit_ms)
            info.stopped = true;
    }
}

static bool is_repetition(const SearchInfo &info, U64 hash) {
    // Search path: any recurrence counts
    for (int i = info.game_ply; i < info.rep_count; i++) {
        if (info.rep_history[i] == hash)
            return true;
    }
    // Game history: 2 occurrences = 3rd time
    int count = 0;
    for (int i = 0; i < info.game_ply; i++) {
        if (info.rep_history[i] == hash) {
            count++;
            if (count >= 2) return true;
        }
    }
    return false;
}

// ===== Quiescence Search =====

static int quiescence(Position &pos, int alpha, int beta, SearchInfo &info, int ply) {
    check_time(info);
    if (info.stopped) return 0;
    if (ply >= MAX_PLY - 1) return evaluate(pos);
    if (pos.halfmove_clock >= 100) return 0;

    info.nodes++;

    int king_sq = lsb(pos.pieces[pos.side_to_move][KING]);
    bool in_check = is_square_attacked(king_sq, pos.side_to_move ^ 1, pos);

    int stand_pat = evaluate(pos);

    if (!in_check) {
        if (stand_pat >= beta) return beta;
        if (stand_pat > alpha) alpha = stand_pat;
    }

    // Delta pruning: if even capturing the most valuable piece can't raise alpha, skip
    if (!in_check && stand_pat + 1000 < alpha) return alpha;

    MoveList ml;
    generate_captures(pos, ml);

    // Sort captures by MVV-LVA
    int scores[256];
    for (int i = 0; i < ml.size(); i++) {
        int flag = get_flag(ml[i]);
        int from = get_from(ml[i]);
        int to = get_to(ml[i]);
        int attacker = find_piece_at(pos, pos.side_to_move, from);
        int victim = (flag == EP_CAPTURE) ? PAWN : find_piece_at(pos, pos.side_to_move ^ 1, to);
        scores[i] = (attacker >= 0 && victim >= 0) ? MVV_LVA[victim][attacker] : 0;
        if (flag >= 8) scores[i] += 1000;
    }
    for (int i = 1; i < ml.size(); i++) {
        Move key = ml[i]; int ks = scores[i]; int j = i - 1;
        while (j >= 0 && scores[j] < ks) { ml[j+1] = ml[j]; scores[j+1] = scores[j]; j--; }
        ml[j+1] = key; scores[j+1] = ks;
    }

    int best_score = in_check ? -INF_SCORE : stand_pat;
    int legal_moves = 0;

    for (int i = 0; i < ml.size(); i++) {
        if (info.stopped) return 0;

        // SEE pruning: skip captures with negative SEE
        if (!in_check && see(pos, ml[i]) < 0)
            continue;

        UndoInfo undo;
        if (!make_move(pos, ml[i], undo))
            continue;

        legal_moves++;
        int score = -quiescence(pos, -beta, -alpha, info, ply + 1);
        unmake_move(pos, ml[i], undo);

        if (info.stopped) return 0;

        if (score > best_score) {
            best_score = score;
            if (score > alpha) {
                alpha = score;
                if (score >= beta) return beta;
            }
        }
    }

    if (in_check && legal_moves == 0)
        return -MATE_SCORE + ply;

    return best_score;
}

// ===== Negamax Search =====

static int negamax(Position &pos, int alpha, int beta, int depth, int ply, SearchInfo &info, bool do_null) {
    info.pv_length[ply] = ply;

    check_time(info);
    if (info.stopped) return 0;
    if (ply >= MAX_PLY - 1) return evaluate(pos);

    // Repetition check
    if (ply > 0 && is_repetition(info, pos.hash))
        return 0;

    // 50-move rule
    if (ply > 0 && pos.halfmove_clock >= 100)
        return 0;

    int king_sq = lsb(pos.pieces[pos.side_to_move][KING]);
    bool in_check = is_square_attacked(king_sq, pos.side_to_move ^ 1, pos);

    // Check extension
    if (in_check && ply < MAX_PLY - 1)
        depth++;

    // Mate distance pruning
    if (ply > 0) {
        alpha = std::max(alpha, -MATE_SCORE + ply);
        beta = std::min(beta, MATE_SCORE - ply - 1);
        if (alpha >= beta) return alpha;
    }

    // TT probe
    Move hash_move = 0;
    TTEntry *entry = tt_probe(pos.hash);
    if (entry && entry->hash == pos.hash) {
        hash_move = entry->best_move;
        if (entry->depth >= depth && ply > 0) {
            int tt_score = entry->score;
            if (tt_score > MATE_SCORE - MAX_PLY) tt_score -= ply;
            else if (tt_score < -MATE_SCORE + MAX_PLY) tt_score += ply;

            if (entry->flag == TT_EXACT) return tt_score;
            if (entry->flag == TT_ALPHA && tt_score <= alpha) return alpha;
            if (entry->flag == TT_BETA && tt_score >= beta) return beta;
        }
    }

    // Leaf node: drop into quiescence
    if (depth <= 0)
        return quiescence(pos, alpha, beta, info, ply);

    info.nodes++;

    int static_eval = in_check ? -INF_SCORE : evaluate(pos);

    // Null move pruning
    if (do_null && !in_check && depth >= 3 && ply > 0 &&
        has_non_pawn_material(pos, pos.side_to_move) &&
        static_eval >= beta) {

        int R = 3 + depth / 6;

        // Make null move
        UndoInfo null_undo;
        null_undo.castling_rights = pos.castling_rights;
        null_undo.en_passant_square = pos.en_passant_square;
        null_undo.halfmove_clock = pos.halfmove_clock;
        null_undo.hash = pos.hash;
        null_undo.move = 0;
        null_undo.captured_piece = -1;

        pos.side_to_move ^= 1;
        pos.hash ^= side_key;
        if (pos.en_passant_square != -1) {
            pos.hash ^= ep_keys[pos.en_passant_square % 8];
            pos.en_passant_square = -1;
        }

        int score = -negamax(pos, -beta, -beta + 1, depth - 1 - R, ply + 1, info, false);

        // Undo null move
        pos.side_to_move ^= 1;
        pos.hash = null_undo.hash;
        pos.en_passant_square = null_undo.en_passant_square;

        if (info.stopped) return 0;

        if (score >= beta && score < MATE_SCORE - MAX_PLY)
            return beta;
    }

    // Generate and sort moves
    MoveList ml;
    generate_moves(pos, ml);
    sort_moves(ml, pos, info, ply, hash_move);

    int legal_moves = 0;
    int best_score = -INF_SCORE;
    Move best_move = 0;
    TTFlag flag = TT_ALPHA;
    int side = pos.side_to_move;

    for (int i = 0; i < ml.size(); i++) {
        if (info.stopped) return 0;

        Move m = ml[i];
        int mflag = get_flag(m);
        bool is_quiet_move = !(mflag & 4) && mflag < 8;

        // Save previous move for counter move heuristic
        Move saved_prev = info.prev_move;
        info.prev_move = m;

        UndoInfo undo;
        if (!make_move(pos, m, undo))
            continue;

        legal_moves++;

        // Push hash to repetition history
        if (info.rep_count < 4096)
            info.rep_history[info.rep_count++] = undo.hash;

        int score;
        if (legal_moves == 1) {
            // Full window search for first move
            score = -negamax(pos, -beta, -alpha, depth - 1, ply + 1, info, true);
        } else {
            // Late Move Reduction (LMR)
            int reduction = 0;
            if (!in_check && legal_moves >= 4 && depth >= 3 && is_quiet_move) {
                // Base reduction
                reduction = 1;
                if (legal_moves >= 8) reduction = 2;
                if (legal_moves >= 16) reduction = 3;

                // Reduce less for killer/counter moves
                if (ply < MAX_PLY && (m == info.killers[ply][0] || m == info.killers[ply][1]))
                    reduction = std::max(0, reduction - 1);

                // Don't reduce into qsearch
                reduction = std::min(reduction, depth - 2);
            }

            // PVS: search with null window first
            score = -negamax(pos, -alpha - 1, -alpha, depth - 1 - reduction, ply + 1, info, true);

            // Re-search if failed high with reduction
            if (score > alpha && reduction > 0)
                score = -negamax(pos, -beta, -alpha, depth - 1, ply + 1, info, true);
            // Re-search with full window if failed high without reduction
            else if (score > alpha && score < beta)
                score = -negamax(pos, -beta, -alpha, depth - 1, ply + 1, info, true);
        }

        // Pop from repetition history
        if (info.rep_count > 0)
            info.rep_count--;

        info.prev_move = saved_prev;
        unmake_move(pos, m, undo);

        if (info.stopped) return 0;

        if (score > best_score) {
            best_score = score;
            best_move = m;

            if (score > alpha) {
                alpha = score;
                flag = TT_EXACT;

                // Update PV
                info.pv[ply][ply] = m;
                for (int j = ply + 1; j < info.pv_length[ply + 1]; j++)
                    info.pv[ply][j] = info.pv[ply + 1][j];
                info.pv_length[ply] = info.pv_length[ply + 1];

                if (score >= beta) {
                    // Update killer moves and history for quiet moves
                    if (is_quiet_move && ply < MAX_PLY) {
                        if (info.killers[ply][0] != m) {
                            info.killers[ply][1] = info.killers[ply][0];
                            info.killers[ply][0] = m;
                        }
                        // History heuristic with depth-based bonus
                        info.history[side][get_from(m)][get_to(m)] += depth * depth;
                        // Clamp to prevent overflow
                        if (info.history[side][get_from(m)][get_to(m)] > 1000000)
                            info.history[side][get_from(m)][get_to(m)] = 1000000;

                        // Counter move
                        if (saved_prev != 0) {
                            int prev_side = side ^ 1;
                            info.counter_moves[prev_side][get_from(saved_prev)][get_to(saved_prev)] = m;
                        }
                    }
                    tt_store(pos.hash, m, score, depth, TT_BETA, ply, info.tt_age);
                    return score;
                }
            }
        }
    }

    if (legal_moves == 0) {
        if (in_check) return -MATE_SCORE + ply;
        return 0; // Stalemate
    }

    tt_store(pos.hash, best_move, best_score, depth, flag, ply, info.tt_age);
    return best_score;
}

// ===== Root search =====

void search_position(Position &pos, SearchInfo &info) {
    info.clear();
    info.start_time_ms = get_time_ms();
    info.stopped = false;

    // Find first legal move as fallback
    MoveList root_moves;
    generate_moves(pos, root_moves);
    Move fallback = 0;
    for (int i = 0; i < root_moves.size(); i++) {
        UndoInfo undo;
        if (make_move(pos, root_moves[i], undo)) {
            fallback = root_moves[i];
            unmake_move(pos, root_moves[i], undo);
            break;
        }
    }
    info.best_move = fallback;

    Move best_move = fallback;
    int best_score = 0;

    // Iterative deepening
    for (int d = 1; d <= info.depth; d++) {
        // Time management: don't start a new iteration if > 50% time used
        if (info.time_limit_ms > 0 && d > 1) {
            int64_t elapsed = get_time_ms() - info.start_time_ms;
            if (elapsed >= info.time_limit_ms * 55 / 100)
                break;
        }

        // Aspiration windows for depth >= 4
        int score;
        if (d >= 4) {
            int delta = 25;
            int alpha = std::max(-INF_SCORE, best_score - delta);
            int beta = std::min(INF_SCORE, best_score + delta);

            while (true) {
                score = negamax(pos, alpha, beta, d, 0, info, true);
                if (info.stopped) break;

                if (score <= alpha) {
                    alpha = std::max(-INF_SCORE, alpha - delta);
                    delta += delta / 2;
                } else if (score >= beta) {
                    beta = std::min(INF_SCORE, beta + delta);
                    delta += delta / 2;
                } else {
                    break;
                }
            }
        } else {
            score = negamax(pos, -INF_SCORE, INF_SCORE, d, 0, info, true);
        }

        if (info.stopped) break;

        if (info.pv_length[0] > 0)
            best_move = info.pv[0][0];
        best_score = score;

        int64_t elapsed = get_time_ms() - info.start_time_ms;
        if (elapsed == 0) elapsed = 1;

        std::cout << "info depth " << d
                  << " score ";

        if (best_score > MATE_SCORE - MAX_PLY)
            std::cout << "mate " << (MATE_SCORE - best_score + 1) / 2;
        else if (best_score < -MATE_SCORE + MAX_PLY)
            std::cout << "mate " << -(MATE_SCORE + best_score + 1) / 2;
        else
            std::cout << "cp " << best_score;

        std::cout << " nodes " << info.nodes
                  << " time " << elapsed
                  << " nps " << (info.nodes * 1000 / elapsed)
                  << " hashfull " << 0
                  << " pv";

        for (int i = 0; i < info.pv_length[0]; i++)
            std::cout << " " << move_to_uci(info.pv[0][i]);

        std::cout << std::endl;
    }

    info.best_move = best_move;
    info.score = best_score;

    std::cout << "bestmove " << move_to_uci(info.best_move) << std::endl;
}
