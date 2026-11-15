#include "perft.h"
#include "zobrist.h"
#include <iostream>
#include <chrono>
#include <vector>

const char *move_to_uci(Move m) {
    if (m == 0) return "(none)";
    static thread_local char s[6];
    int from = get_from(m);
    int to = get_to(m);
    int flag = get_flag(m);
    s[0] = 'a' + (from % 8);
    s[1] = '1' + (from / 8);
    s[2] = 'a' + (to % 8);
    s[3] = '1' + (to / 8);
    int len = 4;
    if (flag >= 8) {
        switch (flag & 3) {
            case 0: s[len++] = 'n'; break;
            case 1: s[len++] = 'b'; break;
            case 2: s[len++] = 'r'; break;
            case 3: s[len++] = 'q'; break;
        }
    }
    s[len] = '\0';
    return s;
}

uint64_t perft(Position &pos, int depth) {
    if (depth == 0) return 1ULL;

    MoveList ml;
    generate_moves(pos, ml);

    uint64_t nodes = 0;
    for (int i = 0; i < ml.size(); i++) {
        UndoInfo undo;
        if (!make_move(pos, ml[i], undo))
            continue;
        nodes += perft(pos, depth - 1);
        unmake_move(pos, ml[i], undo);
    }
    return nodes;
}

uint64_t bench_perft(Position &pos, int depth) {
    return perft(pos, depth);
}

void perft_divide(Position &pos, int depth) {
    if (depth <= 0) return;

    MoveList ml;
    generate_moves(pos, ml);

    uint64_t total = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ml.size(); i++) {
        UndoInfo undo;
        if (!make_move(pos, ml[i], undo))
            continue;
        uint64_t nodes = (depth == 1) ? 1ULL : perft(pos, depth - 1);
        total += nodes;
        std::cout << move_to_uci(ml[i]) << ": " << nodes << "\n";
        unmake_move(pos, ml[i], undo);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "\n====================================\n";
    std::cout << "Depth: " << depth << "\n";
    std::cout << "Total Nodes: " << total << "\n";
    std::cout << "Time: " << ms << " ms";
    if (ms > 0)
        std::cout << " (" << (long long)((double)total / (ms / 1000.0)) << " NPS)";
    std::cout << "\n====================================\n";
}

void run_perft_suite(int max_depth) {
    struct PerftTest {
        const char *name;
        const char *fen;
        int depth;
        uint64_t expected;
    };

    std::vector<PerftTest> suite = {
        {"Startpos d1", START_FEN, 1, 20ULL},
        {"Startpos d2", START_FEN, 2, 400ULL},
        {"Startpos d3", START_FEN, 3, 8902ULL},
        {"Startpos d4", START_FEN, 4, 197281ULL},
        {"Kiwipete d1", KIWIPETE_FEN, 1, 48ULL},
        {"Kiwipete d2", KIWIPETE_FEN, 2, 2039ULL},
        {"Kiwipete d3", KIWIPETE_FEN, 3, 97862ULL},
        {"Kiwipete d4", KIWIPETE_FEN, 4, 4085603ULL},
        {"Pos3 d1", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 1, 14ULL},
        {"Pos3 d2", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 2, 191ULL},
        {"Pos3 d3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2812ULL},
        {"Pos4 d1", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 1, 6ULL},
        {"Pos4 d2", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 2, 264ULL},
        {"Pos4 d3", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467ULL},
    };

    std::cout << "\n========== PERFT SUITE ==========\n";
    int passed = 0, total = 0;

    for (const auto &test : suite) {
        if (test.depth > max_depth) continue;
        total++;
        Position pos;
        load_fen(pos, test.fen);
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t result = perft(pos, test.depth);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        bool ok = (result == test.expected);
        if (ok) passed++;
        std::cout << "[" << (ok ? "PASS" : "FAIL") << "] " << test.name
                  << ": " << result << " (expected " << test.expected << ") in " << ms << "ms\n";
    }
    std::cout << "Summary: " << passed << "/" << total << " passed.\n";
}
