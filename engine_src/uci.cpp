#include "uci.h"
#include "search.h"
#include "movegen.h"
#include "perft.h"
#include "zobrist.h"
#include <iostream>
#include <sstream>
#include <string>

static Position pos;
static SearchInfo info;
static U64 game_history[2048];
static int game_history_count = 0;

static Move parse_move(Position &pos, const std::string &str)
{
    if (str.length() < 4)
        return 0;

    MoveList ml;
    generate_moves(pos, ml);

    int from = (str[0] - 'a') + (str[1] - '1') * 8;
    int to = (str[2] - 'a') + (str[3] - '1') * 8;

    for (int i = 0; i < ml.size(); i++)
    {
        if (get_from(ml[i]) == from && get_to(ml[i]) == to)
        {
            int flag = get_flag(ml[i]);
            if (flag >= 8)
            {
                char promo = (str.length() > 4) ? str[4] : 'q';
                int promo_piece = (flag & 3);
                if ((promo == 'n' && promo_piece == 0) ||
                    (promo == 'b' && promo_piece == 1) ||
                    (promo == 'r' && promo_piece == 2) ||
                    (promo == 'q' && promo_piece == 3))
                {
                    return ml[i];
                }
                continue;
            }
            return ml[i];
        }
    }
    return 0;
}

static void parse_position(std::istringstream &is)
{
    std::string token;
    if (!(is >> token)) return;

    game_history_count = 0;

    if (token == "startpos")
    {
        load_fen(pos, START_FEN);
        is >> token;
    }
    else if (token == "fen")
    {
        std::string fen;
        while (is >> token && token != "moves")
            fen += token + " ";
        load_fen(pos, fen.c_str());
    }

    if (game_history_count < 2048)
        game_history[game_history_count++] = pos.hash;

    while (is >> token)
    {
        if (token == "moves") continue;
        Move m = parse_move(pos, token);
        if (m != 0)
        {
            make_move(pos, m);
            if (game_history_count < 2048)
                game_history[game_history_count++] = pos.hash;
        }
    }
}

static void parse_go(std::istringstream &is)
{
    std::string token;
    int depth = MAX_PLY;
    int64_t wtime = 0, btime = 0, winc = 0, binc = 0;
    int64_t movetime = 0;
    bool infinite = false;
    int perft_depth = 0;

    while (is >> token)
    {
        if (token == "depth") is >> depth;
        else if (token == "wtime") is >> wtime;
        else if (token == "btime") is >> btime;
        else if (token == "winc") is >> winc;
        else if (token == "binc") is >> binc;
        else if (token == "movetime") is >> movetime;
        else if (token == "infinite") infinite = true;
        else if (token == "perft") is >> perft_depth;
    }

    if (perft_depth > 0)
    {
        perft_divide(pos, perft_depth);
        return;
    }

    info.depth = depth;
    info.time_limit_ms = 0;

    if (movetime > 0)
    {
        info.time_limit_ms = movetime;
    }
    else if (!infinite)
    {
        int64_t time_left = (pos.side_to_move == WHITE) ? wtime : btime;
        int64_t inc = (pos.side_to_move == WHITE) ? winc : binc;

        if (time_left > 0)
        {
            info.time_limit_ms = time_left / 30 + inc * 4 / 5;
            if (info.time_limit_ms > time_left - 50)
                info.time_limit_ms = time_left - 50;
            if (info.time_limit_ms < 1)
                info.time_limit_ms = 1;
        }
        else if (depth == MAX_PLY)
        {
            // Default depth if no time or depth specified
            info.depth = 8;
        }
    }

    info.game_ply = game_history_count;
    for (int i = 0; i < game_history_count; i++)
        info.rep_history[i] = game_history[i];
    info.rep_count = game_history_count;

    search_position(pos, info);
}

void uci_loop()
{
    std::string line, token;
    load_fen(pos, START_FEN);

    while (std::getline(std::cin, line))
    {
        std::istringstream is(line);
        is >> token;

        if (token == "uci")
        {
            std::cout << "id name TanmayChess 1.0" << std::endl;
            std::cout << "id author Tanmay" << std::endl;
            std::cout << "option name Hash type spin default 16 min 1 max 1024" << std::endl;
            std::cout << "uciok" << std::endl;
        }
        else if (token == "isready")
        {
            std::cout << "readyok" << std::endl;
        }
        else if (token == "setoption")
        {
            std::string name_tok, opt_name, val_tok;
            int val = 0;
            if (is >> name_tok && name_tok == "name" && is >> opt_name)
            {
                if (opt_name == "Hash" && is >> val_tok && val_tok == "value" && is >> val)
                {
                    init_tt(val);
                }
            }
        }
        else if (token == "ucinewgame")
        {
            clear_tt();
            game_history_count = 0;
            load_fen(pos, START_FEN);
        }
        else if (token == "position")
        {
            parse_position(is);
        }
        else if (token == "go")
        {
            parse_go(is);
        }
        else if (token == "stop")
        {
            info.stopped = true;
        }
        else if (token == "quit")
        {
            break;
        }
        else if (token == "d" || token == "display")
        {
            print_board(pos);
        }
    }
}
