#include "search.h"
#include "evaluation.h"
#include "movegen.h"
#include "perft.h"
#include "zobrist.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>

static std::vector<TTEntry> tt_table;
static size_t tt_mask = 0;

void init_tt(size_t mb)
{
    size_t bytes = mb * 1024 * 1024;
    size_t count = bytes / sizeof(TTEntry);

    // Round down to power of 2
    size_t size = 1;
    while ((size << 1) <= count)
        size <<= 1;

    tt_table.resize(size);
    tt_mask = size - 1;
    clear_tt();
}

void clear_tt()
{
    std::memset(tt_table.data(), 0, tt_table.size() * sizeof(TTEntry));
}

static inline TTEntry* tt_probe(U64 hash)
{
    if (tt_table.empty()) return nullptr;
    return &tt_table[hash & tt_mask];
}

static inline void tt_store(U64 hash, Move best_move, int score, int depth, TTFlag flag, int ply)
{
    if (tt_table.empty()) return;
    TTEntry &entry = tt_table[hash & tt_mask];

    // Store mate score relative to root
    if (score > MATE_SCORE - MAX_PLY)
        score += ply;
    else if (score < -MATE_SCORE + MAX_PLY)
        score -= ply;

    if (entry.hash != hash)
    {
        entry.hash = hash;
        entry.best_move = best_move;
        entry.score = (int32_t)score;
        entry.depth = (int8_t)depth;
        entry.flag = flag;
    }
    else if (depth >= entry.depth || flag == TT_EXACT)
    {
        if (best_move)
            entry.best_move = best_move;
        entry.score = (int32_t)score;
        entry.depth = (int8_t)depth;
        entry.flag = flag;
    }
    else if (best_move)
    {
        entry.best_move = best_move;
    }
}

int64_t get_time_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static const int mvv_lva[6][6] = {
    { 105, 104, 103, 102, 101, 100 },
    { 205, 204, 203, 202, 201, 200 },
    { 305, 304, 303, 302, 301, 300 },
    { 405, 404, 403, 402, 401, 400 },
    { 505, 504, 503, 502, 501, 500 },
    { 605, 604, 603, 602, 601, 600 },
};

static int find_piece_on(const Position &pos, int color, int sq)
{
    for (int p = 0; p < 6; p++)
        if (pos.pieces[color][p] & (1ULL << sq))
            return p;
    return -1;
}

static int score_move(Move m, const Position &pos, const SearchInfo &info, int ply, Move hash_move)
{
    if (m == hash_move)
        return 30000;

    int flag = get_flag(m);
    int from = get_from(m);
    int to = get_to(m);
    int side = pos.side_to_move;

    if (flag & 4)
    {
        int attacker = find_piece_on(pos, side, from);
        int victim = (flag == EP_CAPTURE) ? PAWN : find_piece_on(pos, side ^ 1, to);

        if (attacker >= 0 && victim >= 0)
            return mvv_lva[victim][attacker] + 10000;

        return 10000;
    }

    if (flag >= 8)
        return 9000 + (flag & 3);

    if (ply < MAX_PLY)
    {
        if (m == info.killers[ply][0]) return 8000;
        if (m == info.killers[ply][1]) return 7000;
    }

    return std::min(info.history[side][from][to], 6000);
}

static void sort_moves(MoveList &ml, const Position &pos, const SearchInfo &info, int ply, Move hash_move)
{
    int scores[256];
    for (int i = 0; i < ml.size(); i++)
        scores[i] = score_move(ml[i], pos, info, ply, hash_move);

    for (int i = 0; i < ml.size() - 1; i++)
    {
        int best = i;
        for (int j = i + 1; j < ml.size(); j++)
        {
            if (scores[j] > scores[best])
                best = j;
        }
        if (best != i)
        {
            std::swap(ml[i], ml[best]);
            std::swap(scores[i], scores[best]);
        }
    }
}

static inline void check_time(SearchInfo &info)
{
    if (info.time_limit_ms > 0 && (info.nodes & 1023) == 0)
    {
        if (get_time_ms() - info.start_time_ms >= info.time_limit_ms)
            info.stopped = true;
    }
}

static bool is_repetition(const SearchInfo &info, U64 hash)
{
    // Search path: any recurrence in current search path is a draw
    for (int i = info.game_ply; i < info.rep_count; i++)
    {
        if (info.rep_history[i] == hash)
            return true;
    }

    // Game history: 2 occurrences in game history means this would be the 3rd
    int count = 0;
    for (int i = 0; i < info.game_ply; i++)
    {
        if (info.rep_history[i] == hash)
        {
            count++;
            if (count >= 2)
                return true;
        }
    }
    return false;
}

static int quiescence(Position &pos, int alpha, int beta, SearchInfo &info, int ply)
{
    check_time(info);
    if (info.stopped) return 0;

    if (ply >= MAX_PLY - 1)
        return evaluate(pos);

    if (pos.halfmove_clock >= 100)
        return 0;

    info.nodes++;

    int king_sq = __builtin_ctzll(pos.pieces[pos.side_to_move][KING]);
    bool in_check = is_square_attacked(king_sq, pos.side_to_move ^ 1, pos);

    if (!in_check)
    {
        int stand_pat = evaluate(pos);
        if (stand_pat >= beta)
            return beta;
        if (stand_pat > alpha)
            alpha = stand_pat;
    }

    MoveList ml;
    generate_moves(pos, ml);
    sort_moves(ml, pos, info, 0, 0);

    int legal_moves = 0;
    int best_score = in_check ? -INF_SCORE : alpha;

    for (int i = 0; i < ml.size(); i++)
    {
        if (info.stopped) return 0;
        int flag = get_flag(ml[i]);
        if (!in_check && !(flag & 4) && flag < 8)
            continue;

        Position copy = pos;
        if (!make_move(copy, ml[i]))
            continue;

        legal_moves++;

        int score = -quiescence(copy, -beta, -alpha, info, ply + 1);

        if (info.stopped) return 0;

        if (score > best_score)
        {
            best_score = score;
            if (score > alpha)
            {
                alpha = score;
                if (score >= beta)
                    return beta;
            }
        }
    }

    if (in_check && legal_moves == 0)
        return -MATE_SCORE + ply;

    return in_check ? best_score : alpha;
}

static int negamax(Position &pos, int alpha, int beta, int depth, int ply, SearchInfo &info)
{
    info.pv_length[ply] = ply;

    check_time(info);
    if (info.stopped) return 0;

    if (ply >= MAX_PLY - 1)
        return evaluate(pos);

    // Repetition check (avoid returning draw at root)
    if (ply > 0 && is_repetition(info, pos.hash))
        return 0;

    // 50-move rule
    if (ply > 0 && pos.halfmove_clock >= 100)
        return 0;

    int king_sq = __builtin_ctzll(pos.pieces[pos.side_to_move][KING]);
    bool in_check = is_square_attacked(king_sq, pos.side_to_move ^ 1, pos);

    if (in_check && ply < MAX_PLY - 1)
        depth++;

    Move hash_move = 0;
    TTEntry *entry = tt_probe(pos.hash);
    if (entry && entry->hash == pos.hash)
    {
        hash_move = entry->best_move;
        if (entry->depth >= depth && ply > 0)
        {
            int tt_score = entry->score;
            if (tt_score > MATE_SCORE - MAX_PLY)
                tt_score -= ply;
            else if (tt_score < -MATE_SCORE + MAX_PLY)
                tt_score += ply;

            if (entry->flag == TT_EXACT)
                return tt_score;
            if (entry->flag == TT_ALPHA && tt_score <= alpha)
                return alpha;
            if (entry->flag == TT_BETA && tt_score >= beta)
                return beta;
        }
    }

    if (depth <= 0)
        return quiescence(pos, alpha, beta, info, ply);

    info.nodes++;

    // Null move pruning
    bool has_pieces = (pos.pieces[pos.side_to_move][KNIGHT] |
                       pos.pieces[pos.side_to_move][BISHOP] |
                       pos.pieces[pos.side_to_move][ROOK] |
                       pos.pieces[pos.side_to_move][QUEEN]) != 0;

    if (!in_check && depth >= 3 && ply > 0 && has_pieces)
    {
        Position copy = pos;
        copy.side_to_move ^= 1;
        copy.hash ^= side_key;
        if (copy.en_passant_square != -1)
        {
            copy.hash ^= ep_keys[copy.en_passant_square % 8];
            copy.en_passant_square = -1;
        }

        int score = -negamax(copy, -beta, -beta + 1, depth - 3, ply + 1, info);

        if (info.stopped) return 0;

        if (score >= beta && score < MATE_SCORE - MAX_PLY)
            return beta;
    }

    MoveList ml;
    generate_moves(pos, ml);
    sort_moves(ml, pos, info, ply, hash_move);

    int legal_moves = 0;
    int best_score = -INF_SCORE;
    Move best_move = 0;
    TTFlag flag = TT_ALPHA;

    for (int i = 0; i < ml.size(); i++)
    {
        if (info.stopped) return 0;
        Position copy = pos;
        if (!make_move(copy, ml[i]))
            continue;

        legal_moves++;

        if (info.rep_count < 2048)
            info.rep_history[info.rep_count++] = pos.hash;

        int score;
        if (legal_moves == 1)
        {
            score = -negamax(copy, -beta, -alpha, depth - 1, ply + 1, info);
        }
        else
        {
            int reduction = 0;
            if (!in_check && legal_moves >= 4 && depth >= 3 && !(get_flag(ml[i]) & 4) && get_flag(ml[i]) < 8)
            {
                int enemy_king_sq = __builtin_ctzll(copy.pieces[copy.side_to_move][KING]);
                bool gives_check = is_square_attacked(enemy_king_sq, copy.side_to_move ^ 1, copy);
                if (!gives_check)
                {
                    reduction = 1;
                    if (legal_moves >= 6) reduction = 2;
                }
            }

            int reduced_depth = std::max(0, depth - 1 - reduction);
            score = -negamax(copy, -alpha - 1, -alpha, reduced_depth, ply + 1, info);

            if (score > alpha && (score < beta || reduction > 0))
                score = -negamax(copy, -beta, -alpha, depth - 1, ply + 1, info);
        }

        if (info.rep_count > 0)
            info.rep_count--;

        if (info.stopped) return 0;

        if (score > best_score)
        {
            best_score = score;
            best_move = ml[i];

            if (score > alpha)
            {
                alpha = score;
                flag = TT_EXACT;

                info.pv[ply][ply] = ml[i];
                for (int j = ply + 1; j < info.pv_length[ply + 1]; j++)
                    info.pv[ply][j] = info.pv[ply + 1][j];
                info.pv_length[ply] = info.pv_length[ply + 1];

                if (score >= beta)
                {
                    int mflag = get_flag(ml[i]);
                    if (!(mflag & 4) && mflag < 8 && ply < MAX_PLY)
                    {
                        info.killers[ply][1] = info.killers[ply][0];
                        info.killers[ply][0] = ml[i];
                        info.history[pos.side_to_move][get_from(ml[i])][get_to(ml[i])] += depth * depth;
                    }
                    tt_store(pos.hash, ml[i], score, depth, TT_BETA, ply);
                    return score;
                }
            }
        }
    }

    if (legal_moves == 0)
    {
        if (in_check)
            return -MATE_SCORE + ply;
        return 0;
    }

    tt_store(pos.hash, best_move, best_score, depth, flag, ply);
    return best_score;
}

void search_position(Position &pos, SearchInfo &info)
{
    info.clear();
    info.start_time_ms = get_time_ms();
    info.stopped = false;

    // Fallback: pick the first legal move
    MoveList root_moves;
    generate_moves(pos, root_moves);
    Move fallback = 0;
    for (int i = 0; i < root_moves.size(); i++)
    {
        Position copy = pos;
        if (make_move(copy, root_moves[i]))
        {
            fallback = root_moves[i];
            break;
        }
    }
    info.best_move = fallback;

    Move best_move = fallback;
    int best_score = 0;

    for (int d = 1; d <= info.depth; d++)
    {
        if (info.time_limit_ms > 0 && d > 1)
        {
            int64_t elapsed = get_time_ms() - info.start_time_ms;
            if (elapsed >= info.time_limit_ms / 2)
                break;
        }

        int score = negamax(pos, -INF_SCORE, INF_SCORE, d, 0, info);

        if (info.stopped)
            break;

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
                  << " pv";

        for (int i = 0; i < info.pv_length[0]; i++)
            std::cout << " " << move_to_uci(info.pv[0][i]);

        std::cout << std::endl;
    }

    info.best_move = best_move;
    info.score = best_score;

    std::cout << "bestmove " << move_to_uci(info.best_move) << std::endl;
}
