#include "uci.h"
#include "search.h"
#include "movegen.h"
#include "perft.h"
#include "zobrist.h"
#include "nnue.h"
#include "evaluation.h"
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <cctype>
#include <algorithm>
#include <vector>

static Position pos;
static SearchInfo info;
static U64 game_history[4096];
static int game_history_count = 0;
std::atomic<uint64_t> current_search_id{0};
static std::atomic<bool> search_running{false};
static std::thread search_thread;

static void stop_search(StopReason reason = STOP_USER) {
    info.stopped = true;
    if (reason != STOP_USER) {
        current_search_id.fetch_add(1);
    }

    switch (reason) {
        case STOP_OPTION:
            std::cout << "info string stop requested (option)" << std::endl;
            break;
        case STOP_POSITION:
            std::cout << "info string stop requested (position)" << std::endl;
            break;
        case STOP_NEW_GO:
            std::cout << "info string stop requested (new go)" << std::endl;
            break;
        case STOP_QUIT:
            std::cout << "info string stop requested (quit)" << std::endl;
            break;
        case STOP_USER:
            std::cout << "info string stop requested (user)" << std::endl;
            break;
    }

    if (reason == STOP_QUIT) {
        if (search_thread.joinable()) {
            search_thread.join();
        }
    } else {
        // Bounded wait up to 15ms
        int waited_us = 0;
        while (search_running.load() && waited_us < 15000) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            waited_us += 100;
        }
        if (!search_running.load()) {
            if (search_thread.joinable()) {
                search_thread.join();
            }
        } else {
            // Detach fallback so readline thread never blocks or deadlocks
            if (search_thread.joinable()) {
                std::thread old_thread = std::move(search_thread);
                std::thread([t = std::move(old_thread)]() mutable {
                    if (t.joinable()) t.join();
                }).detach();
            }
        }
    }
}

static Move parse_move(Position &pos, const std::string &str) {
    if (str.length() < 4) return 0;
    MoveList ml;
    generate_moves(pos, ml);
    int from = (str[0] - 'a') + (str[1] - '1') * 8;
    int to = (str[2] - 'a') + (str[3] - '1') * 8;

    for (int i = 0; i < ml.size(); i++) {
        if (get_from(ml[i]) == from && get_to(ml[i]) == to) {
            int flag = get_flag(ml[i]);
            if (flag >= 8) {
                char promo = (str.length() > 4) ? (char)std::tolower(str[4]) : 'q';
                int promo_piece = (flag & 3);
                if ((promo == 'n' && promo_piece == 0) ||
                    (promo == 'b' && promo_piece == 1) ||
                    (promo == 'r' && promo_piece == 2) ||
                    (promo == 'q' && promo_piece == 3)) {
                    UndoInfo undo;
                    if (make_move(pos, ml[i], undo)) {
                        unmake_move(pos, ml[i], undo);
                        return ml[i];
                    }
                }
                continue;
            }
            UndoInfo undo;
            if (make_move(pos, ml[i], undo)) {
                unmake_move(pos, ml[i], undo);
                return ml[i];
            }
        }
    }
    return 0;
}

static void parse_position(std::istringstream &is) {
    std::string token;
    if (!(is >> token)) return;
    game_history_count = 0;

    if (token == "startpos") {
        load_fen(pos, START_FEN);
    } else if (token == "fen") {
        std::string fen;
        while (is >> token && token != "moves")
            fen += token + " ";
        load_fen(pos, fen.c_str());
    }

    if (game_history_count < 4096)
        game_history[game_history_count++] = pos.hash;

    while (is >> token) {
        if (token == "moves") continue;
        Move m = parse_move(pos, token);
        if (m != 0) {
            UndoInfo undo;
            if (make_move(pos, m, undo)) {
                if (game_history_count < 4096)
                    game_history[game_history_count++] = pos.hash;
            }
        }
    }
}

static void parse_go(std::istringstream &is) {
    stop_search(STOP_NEW_GO);
    std::string token;
    int depth = MAX_PLY;
    int64_t wtime = 0, btime = 0, winc = 0, binc = 0;
    int64_t movetime = 0;
    int movestogo = 0;
    bool infinite = false;
    int perft_depth = 0;

    while (is >> token) {
        if (token == "depth") is >> depth;
        else if (token == "wtime") is >> wtime;
        else if (token == "btime") is >> btime;
        else if (token == "winc") is >> winc;
        else if (token == "binc") is >> binc;
        else if (token == "movetime") is >> movetime;
        else if (token == "movestogo") is >> movestogo;
        else if (token == "infinite") infinite = true;
        else if (token == "perft") is >> perft_depth;
    }

    if (perft_depth > 0) {
        perft_divide(pos, perft_depth);
        return;
    }

    info.depth = depth;
    info.time_limit_ms = 0;

    if (movetime > 0) {
        info.time_limit_ms = movetime;
    } else if (!infinite) {
        int64_t time_left = (pos.side_to_move == WHITE) ? wtime : btime;
        int64_t inc = (pos.side_to_move == WHITE) ? winc : binc;

        if (time_left > 0) {
            int divisor = (movestogo > 0) ? (movestogo + 2) : 30;
            info.time_limit_ms = time_left / divisor + inc * 4 / 5;
            if (info.time_limit_ms > time_left - 50)
                info.time_limit_ms = time_left - 50;
            if (info.time_limit_ms < 1)
                info.time_limit_ms = 1;
        } else if (depth == MAX_PLY) {
            info.depth = 10;
        }
    }

    info.game_ply = game_history_count;
    for (int i = 0; i < game_history_count; i++)
        info.rep_history[i] = game_history[i];
    info.rep_count = game_history_count;

    uint64_t search_id = ++current_search_id;
    info.stopped = false;
    search_running = true;

    search_thread = std::thread([pos_copy = pos, search_id]() mutable {
        search_position(pos_copy, info, search_id);
        search_running = false;
    });
}

void uci_loop() {
    std::string line, token;
    load_fen(pos, START_FEN);

    while (std::getline(std::cin, line)) {
        std::istringstream is(line);
        if (!(is >> token)) continue;

        if (token == "uci") {
            std::cout << "id name Chess Engine" << std::endl;
            std::cout << "id author Tanmay" << std::endl;
            std::cout << "option name Hash type spin default 16 min 1 max 1024" << std::endl;
            std::cout << "option name MultiPV type spin default 1 min 1 max 5" << std::endl;
            std::cout << "option name UseNNUE type check default " << (get_use_nnue() ? "true" : "false") << std::endl;
            std::cout << "option name EvalFile type string default nnue.bin" << std::endl;
            std::cout << "option name Style_Calculation type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Style_Aggression type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Style_Structure type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Style_Creativity type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Style_Defense type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Style_Prophylaxis type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Style_Activity type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Style_Preset type combo default Balanced var Balanced var Tal var Petrosian var Karpov var Morphy var Kasparov var Carlsen var Custom" << std::endl;
            std::cout << "option name Nen_Enhancement type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Nen_Transmutation type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Nen_Conjuration type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Nen_Specialization type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Nen_Manipulation type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Nen_Emission type spin default 50 min 0 max 100" << std::endl;
            std::cout << "option name Nen_Preset type combo default Balanced var Balanced var Tal var Petrosian var Karpov var Morphy var Kasparov var Carlsen var Custom" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        else if (token == "isready") {
            std::cout << "readyok" << std::endl;
        }
        else if (token == "setoption") {
            std::string name_tok, opt_name, val_tok;
            if (is >> name_tok && name_tok == "name" && is >> opt_name) {
                if (opt_name == "Hash" && is >> val_tok && val_tok == "value") {
                    int val = 16;
                    if (is >> val) { stop_search(STOP_OPTION); init_tt(val); }
                }
                else if (opt_name == "MultiPV" && is >> val_tok && val_tok == "value") {
                    int val = 1;
                    if (is >> val) { stop_search(STOP_OPTION); info.multipv = std::clamp(val, 1, 5); }
                }
                else if (opt_name == "UseNNUE" && is >> val_tok && val_tok == "value") {
                    std::string val;
                    if (is >> val) { stop_search(STOP_OPTION); set_use_nnue(val == "true"); }
                }
                else if (opt_name == "EvalFile" && is >> val_tok && val_tok == "value") {
                    std::string filename;
                    if (is >> filename) { stop_search(STOP_OPTION); load_nnue_file(filename.c_str()); }
                }
                else if ((opt_name.rfind("Style_", 0) == 0 || opt_name.rfind("Nen_", 0) == 0) && is >> val_tok && val_tok == "value") {
                    stop_search(STOP_OPTION);
                    if (get_use_nnue()) {
                        set_use_nnue(false);
                        std::cout << "info string Style options active, NNUE disabled (classical playstyle eval)" << std::endl;
                    }
                    NenProfile prof = get_nen_profile();
                    if (opt_name == "Style_Preset" || opt_name == "Nen_Preset") {
                        std::string preset;
                        if (is >> preset) {
                            if (preset == "Tal") prof = TAL_PROFILE;
                            else if (preset == "Petrosian") prof = PETROSIAN_PROFILE;
                            else if (preset == "Karpov") prof = KARPOV_PROFILE;
                            else if (preset == "Morphy") prof = MORPHY_PROFILE;
                            else if (preset == "Kasparov") prof = KASPAROV_PROFILE;
                            else if (preset == "Carlsen") prof = CARLSEN_PROFILE;
                            else if (preset == "Balanced") prof = BALANCED_PROFILE;
                        }
                    } else {
                        int val = 50;
                        if (is >> val) {
                            val = std::clamp(val, 0, 100);
                            if (opt_name == "Style_Calculation" || opt_name == "Nen_Enhancement") prof.enhancement = val;
                            else if (opt_name == "Style_Aggression" || opt_name == "Nen_Transmutation") prof.transmutation = val;
                            else if (opt_name == "Style_Structure" || opt_name == "Nen_Conjuration") prof.conjuration = val;
                            else if (opt_name == "Style_Creativity" || opt_name == "Nen_Specialization") prof.specialization = val;
                            else if (opt_name == "Style_Defense" || opt_name == "Style_Prophylaxis" || opt_name == "Nen_Manipulation") prof.manipulation = val;
                            else if (opt_name == "Style_Activity" || opt_name == "Nen_Emission") prof.emission = val;
                        }
                    }
                    set_nen_profile(prof);
                    clear_tt();
                }
            }
        }
        else if (token == "ucinewgame") {
            stop_search(STOP_POSITION);
            clear_tt();
            info.clear();
            game_history_count = 0;
            load_fen(pos, START_FEN);
        }
        else if (token == "position") {
            stop_search(STOP_POSITION);
            parse_position(is);
        }
        else if (token == "go") {
            parse_go(is);
        }
        else if (token == "stop") {
            stop_search(STOP_USER);
        }
        else if (token == "quit") {
            stop_search(STOP_QUIT);
            break;
        }
        else if (token == "d" || token == "display") {
            stop_search(STOP_USER);
            print_board(pos);
        }
        else if (token == "fen") {
            stop_search(STOP_USER);
            std::cout << "fen " << export_fen(pos) << std::endl;
        }
        else if (token == "eval") {
            stop_search(STOP_USER);
            int score = evaluate(pos);
            std::cout << "Evaluation: " << score << " cp (from " << (pos.side_to_move == WHITE ? "white" : "black") << "'s perspective)" << std::endl;
        }
        else if (token == "evalfen") {
            std::string fen_str;
            std::getline(is, fen_str);
            size_t first = fen_str.find_first_not_of(' ');
            if (first != std::string::npos) {
                Position tmp;
                load_fen(tmp, fen_str.substr(first).c_str());
                std::cout << evaluate(tmp) << '\n';
            }
        }
        else if (token == "bench") {
            stop_search(STOP_USER);
            // Run a benchmark: search several positions to fixed depth
            const char *bench_fens[] = {
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
                "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
                "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
                "3r2k1/pp3ppp/2n5/8/3P4/2P5/P1Q2PPP/R4RK1 w - - 0 1",
                "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
            };
            int bench_depth = 8;
            if (is >> token) bench_depth = std::stoi(token);

            uint64_t total_nodes = 0;
            auto start = get_time_ms();

            for (int i = 0; i < 8; i++) {
                load_fen(pos, bench_fens[i]);
                clear_tt();
                info.depth = bench_depth;
                info.time_limit_ms = 0;
                info.game_ply = 0;
                info.rep_count = 0;
                search_position(pos, info);
                total_nodes += info.nodes;
            }

            auto elapsed = get_time_ms() - start;
            if (elapsed == 0) elapsed = 1;
            std::cout << "\n=============================\n";
            std::cout << "Bench complete: " << total_nodes << " nodes, "
                      << elapsed << " ms, " << (total_nodes * 1000 / elapsed) << " nps\n";
        }
        else if (token == "perftsuite") {
            stop_search();
            int max_d = 4;
            if (is >> token) max_d = std::stoi(token);
            run_perft_suite(max_d);
        }
        else if (token == "legalmoves") {
            stop_search();
            MoveList ml;
            generate_moves(pos, ml);
            std::cout << "legalmoves";
            for (int i = 0; i < ml.size(); i++) {
                UndoInfo undo;
                if (make_move(pos, ml[i], undo)) {
                    std::cout << " " << move_to_uci(ml[i]);
                    unmake_move(pos, ml[i], undo);
                }
            }
            std::cout << std::endl;
        }
        else if (token == "state") {
            stop_search();
            int side = pos.side_to_move;
            int enemy = side ^ 1;
            int king_sq = (pos.pieces[side][KING] != 0ULL) ? lsb(pos.pieces[side][KING]) : -1;
            bool in_check = (king_sq != -1) && is_square_attacked(king_sq, enemy, pos);

            MoveList ml;
            generate_moves(pos, ml);
            std::vector<std::string> legal;
            for (int i = 0; i < ml.size(); i++) {
                UndoInfo undo;
                if (make_move(pos, ml[i], undo)) {
                    legal.push_back(move_to_uci(ml[i]));
                    unmake_move(pos, ml[i], undo);
                }
            }

            bool checkmate = (legal.empty() && in_check);
            bool stalemate = (legal.empty() && !in_check);
            std::string fen = export_fen(pos);

            std::cout << "state fen " << fen
                      << " side " << (side == WHITE ? "w" : "b")
                      << " check " << (in_check ? 1 : 0)
                      << " mate " << (checkmate ? 1 : 0)
                      << " stalemate " << (stalemate ? 1 : 0)
                      << " halfmove " << pos.halfmove_clock
                      << " fullmove " << pos.fullmove_number
                      << " legalmoves";
            for (const auto &m : legal) std::cout << " " << m;
            std::cout << std::endl;
        }
    }
    stop_search();
}
