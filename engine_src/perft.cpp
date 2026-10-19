#include "perft.h"
#include <iostream>
#include <chrono>
#include <vector>

const char *move_to_uci(Move m)
{
    static thread_local char s[6];
    int from = get_from(m);
    int to = get_to(m);
    int flag = get_flag(m);

    s[0] = (char)('a' + (from % 8));
    s[1] = (char)('1' + (from / 8));
    s[2] = (char)('a' + (to % 8));
    s[3] = (char)('1' + (to / 8));
    int len = 4;

    // Handle promotions: flags 8..11 (quiet) and 12..15 (capture)
    if (flag >= 8)
    {
        switch (flag & 3)
        {
        case 0: s[len++] = 'n'; break;
        case 1: s[len++] = 'b'; break;
        case 2: s[len++] = 'r'; break;
        case 3: s[len++] = 'q'; break;
        }
    }
    s[len] = '\0';

    return s;
}


uint64_t perft(Position &pos, int depth)
{
    if (depth == 0)
        return 1ULL;

    MoveList move_list;
    generate_moves(pos, move_list);

    uint64_t nodes = 0;
    for (int i = 0; i < move_list.size(); i++)
    {
        Position copy = pos;
        if (!make_move(copy, move_list[i]))
            continue; // illegal move (e.g. king left in check)

        nodes += perft(copy, depth - 1);
    }

    return nodes;
}

void perft_divide(Position &pos, int depth)
{
    if (depth <= 0)
        return;

    MoveList move_list;
    generate_moves(pos, move_list);

    uint64_t total_nodes = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < move_list.size(); i++)
    {
        Move m = move_list[i];
        Position copy = pos;
        if (!make_move(copy, m))
            continue; // illegal move

        uint64_t nodes = (depth == 1) ? 1ULL : perft(copy, depth - 1);
        total_nodes += nodes;

        std::cout << move_to_uci(m) << ": " << nodes << "\n";
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "\n====================================\n";
    std::cout << "Depth: " << depth << "\n";
    std::cout << "Total Nodes: " << total_nodes << "\n";
    std::cout << "Time: " << elapsed_ms << " ms";
    if (elapsed_ms > 0)
    {
        double nps = (double)total_nodes / (elapsed_ms / 1000.0);
        std::cout << " (" << (long long)nps << " NPS)";
    }
    std::cout << "\n====================================\n";
}

void run_perft_suite(int max_depth)
{
    std::vector<PerftTest> suite = {
        {"Start Position (Depth 1)", START_FEN, 1, 20ULL},
        {"Start Position (Depth 2)", START_FEN, 2, 400ULL},
        {"Start Position (Depth 3)", START_FEN, 3, 8902ULL},
        {"Start Position (Depth 4)", START_FEN, 4, 197281ULL},
        {"Kiwipete (Depth 1)", KIWIPETE_FEN, 1, 48ULL},
        {"Kiwipete (Depth 2)", KIWIPETE_FEN, 2, 2039ULL},
        {"Kiwipete (Depth 3)", KIWIPETE_FEN, 3, 97862ULL},
        {"Kiwipete (Depth 4)", KIWIPETE_FEN, 4, 4085603ULL},
        {"Position 3 (Depth 1)", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 1, 14ULL},
        {"Position 3 (Depth 2)", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 2, 191ULL},
        {"Position 3 (Depth 3)", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2812ULL},
        {"Position 4 (Depth 1)", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 1, 6ULL},
        {"Position 4 (Depth 2)", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 2, 264ULL},
        {"Position 4 (Depth 3)", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467ULL}
    };

    std::cout << "\n========== RUNNING PERFT SUITE ==========\n";
    int passed = 0;
    int total = 0;

    for (const auto &test : suite)
    {
        if (test.depth > max_depth)
            continue;

        total++;
        Position pos;
        load_fen(pos, test.fen);

        auto start = std::chrono::high_resolution_clock::now();
        uint64_t result = perft(pos, test.depth);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        bool ok = (result == test.expected_nodes);
        if (ok) passed++;

        std::cout << "[" << (ok ? "PASS" : "FAIL") << "] " 
                  << test.name << ": " << result 
                  << " (Expected: " << test.expected_nodes << ") in " << ms << " ms\n";
    }

    std::cout << "Summary: " << passed << "/" << total << " tests passed.\n";
    std::cout << "=========================================\n";
}
