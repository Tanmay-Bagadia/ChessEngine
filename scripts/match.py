#!/usr/bin/env python3
"""
scripts/match.py: 1v1 Chess Engine Match & Elo Benchmark Runner

Runs head-to-head matches between any two UCI chess engines (your engine in
NNUE or Classical/Playstyle mode vs Stockfish, GNU Chess, LCZero/Maia, or
another preset of your engine).

Examples:
  # 1) Test your NNUE model vs Classical PeSTO (6 games, varied openings)
  .venv/bin/python scripts/match.py --e1-nnue --e1-name "NNUE-v2" --e2-name "PeSTO" --movetime 0.3 --games 6 --openings

  # 2) Benchmark your engine vs Stockfish at 1500 Elo (brew install stockfish)
  .venv/bin/python scripts/match.py --e1-nnue --e2 /opt/homebrew/bin/stockfish --e2-elo 1500 --movetime 0.3 --games 6 --openings --pgn sf1500.pgn

  # 3) Test Tal style vs Petrosian style
  .venv/bin/python scripts/match.py --e1-style Tal --e2-style Petrosian --movetime 0.3 --games 4 --openings
"""

import argparse
import math
import os
import sys
import time
import chess
import chess.engine
import chess.pgn

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ENGINE = os.path.abspath(os.path.join(SCRIPT_DIR, '..', 'build', 'engine'))

# Standard balanced opening lines (played as White/Black pairs when --openings is set)
OPENING_BOOK = [
    ("Italian Game",        ["e2e4", "e7e5", "g1f3", "b8c6", "f1c4"]),
    ("Sicilian Najdorf",    ["e2e4", "c7c5", "g1f3", "d7d6", "d2d4", "c5d4", "f3d4", "g8f6", "b1c3"]),
    ("Queen's Gambit",      ["d2d4", "d7d5", "c2c4", "e7e6", "b1c3", "g8f6"]),
    ("Ruy Lopez",           ["e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6"]),
    ("French Defense",      ["e2e4", "e7e6", "d2d4", "d7d5", "b1c3"]),
    ("Caro-Kann Defense",   ["e2e4", "c7c6", "d2d4", "d7d5", "b1c3"]),
    ("King's Indian",       ["d2d4", "g8f6", "c2c4", "g7g6", "b1c3", "f8g7"]),
    ("English Opening",     ["c2c4", "e7e5", "b1c3", "g8f6"]),
]


def parse_args():
    parser = argparse.ArgumentParser(description="1v1 UCI Chess Engine Match & Benchmark Runner")
    parser.add_argument("--e1", default=DEFAULT_ENGINE, help="Path to Engine 1 binary (default: ./build/engine)")
    parser.add_argument("--e2", default=DEFAULT_ENGINE, help="Path to Engine 2 binary (default: ./build/engine)")
    parser.add_argument("--e1-name", default=None, help="Display name for Engine 1")
    parser.add_argument("--e2-name", default=None, help="Display name for Engine 2")
    parser.add_argument("--e1-style", default=None, help="Playstyle preset for Engine 1 (Tal, Petrosian, Karpov, Morphy, Kasparov, Carlsen, Balanced)")
    parser.add_argument("--e2-style", default=None, help="Playstyle preset for Engine 2")
    parser.add_argument("--e1-nnue", action="store_true", help="Enable NNUE for Engine 1")
    parser.add_argument("--e2-nnue", action="store_true", help="Enable NNUE for Engine 2")
    parser.add_argument("--e1-elo", type=int, default=None, help="Limit Engine 1 strength via UCI_Elo (e.g. Stockfish 1320..3190)")
    parser.add_argument("--e2-elo", type=int, default=None, help="Limit Engine 2 strength via UCI_Elo (e.g. Stockfish 1320..3190)")
    parser.add_argument("--e1-opt", action="append", default=[], help="Extra UCI option for Engine 1 as Key=Value (repeatable)")
    parser.add_argument("--e2-opt", action="append", default=[], help="Extra UCI option for Engine 2 as Key=Value (repeatable)")
    parser.add_argument("--games", type=int, default=2, help="Number of games to play (alternating colors)")
    parser.add_argument("--movetime", type=float, default=None, help="Seconds per move (fixed movetime, default 0.5s if no --time/--depth)")
    parser.add_argument("--time", type=float, default=None, help="Base time in seconds per player (clock mode)")
    parser.add_argument("--inc", type=float, default=0.1, help="Increment in seconds per move (clock mode)")
    parser.add_argument("--depth", type=int, default=None, help="Fixed search depth per move")
    parser.add_argument("--openings", action="store_true", help="Use varied standard opening book pairs so games don't repeat")
    parser.add_argument("--pgn", default=None, help="Output PGN filename to record games")
    parser.add_argument("--quiet", action="store_true", help="Suppress move-by-move output")
    return parser.parse_args()


def parse_opt_value(val_str):
    low = val_str.lower()
    if low == "true":
        return True
    if low == "false":
        return False
    try:
        return int(val_str)
    except ValueError:
        return val_str


def configure_engine(transport, name_label, style=None, use_nnue=False, elo=None, extra_opts=None):
    """Sends UCI options (Style presets, NNUE, UCI_Elo, custom options) to an engine."""
    options = {}
    if use_nnue:
        options["UseNNUE"] = True
    elif style:
        options["UseNNUE"] = False
        options["Style_Preset"] = style

    if elo is not None:
        options["UCI_LimitStrength"] = True
        options["UCI_Elo"] = elo

    for item in (extra_opts or []):
        if "=" in item:
            k, v = item.split("=", 1)
            options[k.strip()] = parse_opt_value(v.strip())

    for opt_name, val in options.items():
        if opt_name in transport.options:
            try:
                transport.configure({opt_name: val})
            except Exception as e:
                print(f"Warning: could not set {opt_name}={val} on {name_label}: {e}")
        elif opt_name in ("UCI_LimitStrength", "UCI_Elo"):
            print(f"Warning: {name_label} does not support UCI option '{opt_name}'")


def play_game(engine_white, engine_black, name_white, name_black, args, game_num, opening=None):
    board = chess.Board()
    pgn_game = chess.pgn.Game()
    pgn_game.headers["Event"] = "1v1 Engine Match"
    pgn_game.headers["Round"] = str(game_num)
    pgn_game.headers["White"] = name_white
    pgn_game.headers["Black"] = name_black
    pgn_game.headers["Date"] = time.strftime("%Y.%m.%d")

    node = pgn_game
    if opening is not None:
        op_name, op_moves = opening
        pgn_game.headers["Opening"] = op_name
        for uci in op_moves:
            m = chess.Move.from_uci(uci)
            board.push(m)
            node = node.add_variation(m)

    white_clock = args.time if args.time else None
    black_clock = args.time if args.time else None

    if not args.quiet:
        op_suffix = f" | Opening: {opening[0]}" if opening else ""
        print(f"\n========================================================")
        print(f"🏁 GAME {game_num}: [White] {name_white} vs [Black] {name_black}{op_suffix}")
        print(f"========================================================")

    ply = len(board.move_stack)
    while not board.is_game_over(claim_draw=True) and ply < 300:
        ply += 1
        is_white = (board.turn == chess.WHITE)
        current_engine = engine_white if is_white else engine_black

        if args.time is not None:
            limit = chess.engine.Limit(
                white_clock=white_clock, black_clock=black_clock,
                white_inc=args.inc, black_inc=args.inc
            )
        elif args.depth is not None:
            limit = chess.engine.Limit(depth=args.depth)
        else:
            mt = args.movetime if args.movetime is not None else 0.5
            limit = chess.engine.Limit(time=mt)

        start_t = time.time()
        result = current_engine.play(board, limit)
        elapsed = time.time() - start_t

        move = result.move
        if not move or move not in board.legal_moves:
            print(f"⚠️  Illegal or null move from {'White' if is_white else 'Black'}: {move}")
            break

        san_move = board.san(move)
        board.push(move)
        node = node.add_variation(move)

        if not args.quiet:
            turn_str = f"{(ply + 1) // 2}." if is_white else f"{(ply + 1) // 2}..."
            player_str = name_white if is_white else name_black
            print(f"  {turn_str:<6} {san_move:<8} ({move.uci()}) by {player_str:<20} [{elapsed*1000:5.0f}ms]")

        if args.time is not None:
            if is_white:
                white_clock = max(0.01, white_clock - elapsed + args.inc)
            else:
                black_clock = max(0.01, black_clock - elapsed + args.inc)

    res = board.result(claim_draw=True)
    if res == "*":
        res = "1/2-1/2"
    pgn_game.headers["Result"] = res

    reason = "Adjudicated draw (move limit)"
    if board.is_checkmate():
        winner = name_black if board.turn == chess.WHITE else name_white
        reason = f"Checkmate! {winner} wins."
    elif board.is_stalemate():
        reason = "Draw by stalemate"
    elif board.is_insufficient_material():
        reason = "Draw by insufficient material"
    elif board.can_claim_threefold_repetition():
        reason = "Draw by threefold repetition"
    elif board.can_claim_fifty_moves():
        reason = "Draw by 50-move rule"

    print(f"Game {game_num} Result: {res} ({reason}) in {ply} plies.")
    return res, pgn_game


def main():
    args = parse_args()

    if not os.path.isfile(args.e1):
        print(f"Error: Engine 1 binary not found at '{args.e1}'")
        sys.exit(1)
    if not os.path.isfile(args.e2):
        print(f"Error: Engine 2 binary not found at '{args.e2}'")
        print("Tip: Install Stockfish with: brew install stockfish")
        sys.exit(1)

    def default_name(path, style, nnue, elo):
        base = os.path.basename(path)
        tags = []
        if nnue:
            tags.append("NNUE")
        elif style:
            tags.append(style)
        if elo is not None:
            tags.append(f"Elo {elo}")
        return f"{base} ({', '.join(tags)})" if tags else base

    name1 = args.e1_name or default_name(args.e1, args.e1_style, args.e1_nnue, args.e1_elo)
    name2 = args.e2_name or default_name(args.e2, args.e2_style, args.e2_nnue, args.e2_elo)
    if name1 == name2:
        name1 += "-A"
        name2 += "-B"

    print("========================================================")
    print("🏆 1v1 CHESS ENGINE MATCH")
    print(f"Engine 1: {name1} [{args.e1}]")
    print(f"Engine 2: {name2} [{args.e2}]")
    print(f"Games   : {args.games} (alternating colors{', book openings' if args.openings else ''})")
    if args.time is not None:
        print(f"Control : {args.time}s + {args.inc}s increment")
    elif args.depth is not None:
        print(f"Control : depth {args.depth}")
    else:
        print(f"Control : {args.movetime if args.movetime is not None else 0.5}s per move")
    print("========================================================")

    e1 = chess.engine.SimpleEngine.popen_uci(args.e1)
    e2 = chess.engine.SimpleEngine.popen_uci(args.e2)

    configure_engine(e1, name1, style=args.e1_style, use_nnue=args.e1_nnue, elo=args.e1_elo, extra_opts=args.e1_opt)
    configure_engine(e2, name2, style=args.e2_style, use_nnue=args.e2_nnue, elo=args.e2_elo, extra_opts=args.e2_opt)

    score1 = 0.0
    score2 = 0.0
    wins1 = draws = losses1 = 0
    all_pgns = []

    try:
        for g in range(1, args.games + 1):
            opening = OPENING_BOOK[((g - 1) // 2) % len(OPENING_BOOK)] if args.openings else None
            if g % 2 == 1:
                res, pgn = play_game(e1, e2, name1, name2, args, g, opening=opening)
                if res == "1-0":
                    score1 += 1.0; wins1 += 1
                elif res == "0-1":
                    score2 += 1.0; losses1 += 1
                else:
                    score1 += 0.5; score2 += 0.5; draws += 1
            else:
                res, pgn = play_game(e2, e1, name2, name1, args, g, opening=opening)
                if res == "1-0":
                    score2 += 1.0; losses1 += 1
                elif res == "0-1":
                    score1 += 1.0; wins1 += 1
                else:
                    score1 += 0.5; score2 += 0.5; draws += 1

            all_pgns.append(pgn)
            print(f"Standings: {name1} {score1:.1f} - {score2:.1f} {name2} (+{wins1} ={draws} -{losses1})")

        win_rate = score1 / max(args.games, 1)
        if 0.0 < win_rate < 1.0:
            elo_diff = -400.0 * math.log10(1.0 / win_rate - 1.0)
            elo_str = f"{elo_diff:+.0f} Elo"
        elif win_rate == 1.0:
            elo_str = "+inf (100% score)"
        else:
            elo_str = "-inf (0% score)"

        print("\n========================================================")
        print(f"🏆 FINAL RESULT: {name1} {score1:.1f} — {score2:.1f} {name2} (+{wins1} ={draws} -{losses1})")
        print(f"📊 Performance : {win_rate * 100:.1f}% ({elo_str})")
        print("========================================================")

        if args.pgn:
            with open(args.pgn, "w") as f:
                for p in all_pgns:
                    print(p, file=f, end="\n\n")
            print(f"💾 Saved {len(all_pgns)} game(s) to '{args.pgn}'")

    finally:
        e1.quit()
        e2.quit()


if __name__ == "__main__":
    main()
