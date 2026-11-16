#!/usr/bin/env python3
"""
scripts/match.py: 1v1 Chess Engine Match Runner

Runs head-to-head matches between two UCI chess engines using python-chess.
Supports opening FENs, time controls, style/Nen options, and exports PGN results.

Usage examples:
  # Internal benchmark: Tal style vs Petrosian style (1 second per move, 2 games)
  python3 scripts/match.py --e1-style Tal --e2-style Petrosian --movetime 1.0 --games 2

  # Match against installed Stockfish (brew install stockfish)
  python3 scripts/match.py --e2 /opt/homebrew/bin/stockfish --movetime 0.5 --games 4 --pgn match.pgn

  # Fast time control match: 10s + 0.1s increment
  python3 scripts/match.py --e2 /opt/homebrew/bin/stockfish --time 10.0 --inc 0.1 --games 2
"""

import argparse
import os
import sys
import time
import chess
import chess.engine
import chess.pgn

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ENGINE = os.path.abspath(os.path.join(SCRIPT_DIR, '..', 'build', 'engine'))


def parse_args():
    parser = argparse.ArgumentParser(description="1v1 UCI Chess Engine Match Runner")
    parser.add_argument("--e1", default=DEFAULT_ENGINE, help="Path to Engine 1 binary (default: ./build/engine)")
    parser.add_argument("--e2", default=DEFAULT_ENGINE, help="Path to Engine 2 binary (default: ./build/engine or stockfish)")
    parser.add_argument("--e1-name", default=None, help="Display name for Engine 1")
    parser.add_argument("--e2-name", default=None, help="Display name for Engine 2")
    parser.add_argument("--e1-style", default=None, help="Nen/Style preset for Engine 1 (e.g. Tal, Petrosian, Kasparov)")
    parser.add_argument("--e2-style", default=None, help="Nen/Style preset for Engine 2 (e.g. Morphy, Carlsen, Balanced)")
    parser.add_argument("--e1-nnue", action="store_true", help="Enable NNUE for Engine 1")
    parser.add_argument("--e2-nnue", action="store_true", help="Enable NNUE for Engine 2")
    parser.add_argument("--games", type=int, default=2, help="Number of games to play (alternating colors)")
    parser.add_argument("--movetime", type=float, default=0.5, help="Seconds per move (fixed movetime)")
    parser.add_argument("--time", type=float, default=None, help="Base time in seconds per player (clock mode)")
    parser.add_argument("--inc", type=float, default=0.1, help="Increment in seconds per move (clock mode)")
    parser.add_argument("--depth", type=int, default=None, help="Fixed search depth per move")
    parser.add_argument("--pgn", default=None, help="Output PGN filename to record games")
    parser.add_argument("--quiet", action="store_true", help="Suppress move-by-move output")
    return parser.parse_args()


def configure_engine(transport, name_label, style=None, use_nnue=False):
    """Sends custom UCI options (Style presets, NNUE) to an engine."""
    options = {}
    if use_nnue:
        options["UseNNUE"] = True
    elif style:
        options["UseNNUE"] = False
        options["Style_Preset"] = style

    for opt_name, val in options.items():
        try:
            transport.configure({opt_name: val})
        except Exception as e:
            pass  # Ignored if the engine (e.g. Stockfish) doesn't have Style_* options


def play_game(engine_white, engine_black, name_white, name_black, args, game_num):
    board = chess.Board()
    pgn_game = chess.pgn.Game()
    pgn_game.headers["Event"] = "1v1 Engine Match"
    pgn_game.headers["Round"] = str(game_num)
    pgn_game.headers["White"] = name_white
    pgn_game.headers["Black"] = name_black
    pgn_game.headers["Date"] = time.strftime("%Y.%m.%d")

    node = pgn_game
    white_clock = args.time if args.time else None
    black_clock = args.time if args.time else None

    if not args.quiet:
        print(f"\n========================================================")
        print(f"🏁 GAME {game_num}: [White] {name_white} vs [Black] {name_black}")
        print(f"========================================================")

    ply = 0
    while not board.is_game_over(claim_draw=True):
        ply += 1
        is_white = (board.turn == chess.WHITE)
        current_engine = engine_white if is_white else engine_black

        # Configure time limit
        if args.movetime:
            limit = chess.engine.Limit(time=args.movetime)
        elif args.depth:
            limit = chess.engine.Limit(depth=args.depth)
        elif args.time:
            clock = white_clock if is_white else black_clock
            limit = chess.engine.Limit(white_clock=white_clock, black_clock=black_clock,
                                       white_inc=args.inc, black_inc=args.inc)
        else:
            limit = chess.engine.Limit(time=0.5)

        start_t = time.time()
        result = current_engine.play(board, limit)
        elapsed = time.time() - start_t

        move = result.move
        if not move or move not in board.legal_moves:
            # Engine forfeited or illegal move
            print(f"⚠️  Illegal or null move from {'White' if is_white else 'Black'}: {move}")
            break

        san_move = board.san(move)
        board.push(move)
        node = node.add_variation(move)

        if not args.quiet:
            turn_str = f"{(ply + 1) // 2}." if is_white else f"{(ply + 1) // 2}..."
            player_str = name_white if is_white else name_black
            print(f"  {turn_str:<6} {san_move:<8} ({move.uci()}) by {player_str:<18} [{elapsed*1000:5.0f}ms]")

        # Update clock
        if args.time:
            if is_white:
                white_clock = max(0.01, white_clock - elapsed + args.inc)
            else:
                black_clock = max(0.01, black_clock - elapsed + args.inc)

    res = board.result(claim_draw=True)
    pgn_game.headers["Result"] = res
    reason = "Normal termination"
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

    print(f"\nResult: {res} ({reason}) in {ply} plies.")
    return res, pgn_game


def main():
    args = parse_args()

    if not os.path.isfile(args.e1):
        print(f"Error: Engine 1 binary not found at '{args.e1}'")
        sys.exit(1)
    if not os.path.isfile(args.e2):
        print(f"Error: Engine 2 binary not found at '{args.e2}'")
        print("Tip: To play against Stockfish, install it via: brew install stockfish")
        sys.exit(1)

    name1 = args.e1_name or (f"Engine1 ({args.e1_style})" if args.e1_style else os.path.basename(args.e1))
    name2 = args.e2_name or (f"Engine2 ({args.e2_style})" if args.e2_style else os.path.basename(args.e2))
    if name1 == name2:
        name1 += "-A"
        name2 += "-B"

    print("========================================================")
    print("🏆 1v1 CHESS ENGINE MATCH")
    print(f"Engine 1: {name1} [{args.e1}]")
    print(f"Engine 2: {name2} [{args.e2}]")
    print(f"Games   : {args.games} (alternating colors)")
    if args.movetime:
        print(f"Control : {args.movetime}s per move")
    elif args.time:
        print(f"Control : {args.time}s + {args.inc}s increment")
    elif args.depth:
        print(f"Control : depth {args.depth}")
    print("========================================================")

    # Spawn engines
    e1 = chess.engine.SimpleEngine.popen_uci(args.e1)
    e2 = chess.engine.SimpleEngine.popen_uci(args.e2)

    configure_engine(e1, name1, style=args.e1_style, use_nnue=args.e1_nnue)
    configure_engine(e2, name2, style=args.e2_style, use_nnue=args.e2_nnue)

    score1 = 0.0
    score2 = 0.0
    all_pgns = []

    try:
        for g in range(1, args.games + 1):
            if g % 2 == 1:
                # E1 is White, E2 is Black
                res, pgn = play_game(e1, e2, name1, name2, args, g)
                if res == "1-0": score1 += 1.0
                elif res == "0-1": score2 += 1.0
                else: score1 += 0.5; score2 += 0.5
            else:
                # E2 is White, E1 is Black
                res, pgn = play_game(e2, e1, name2, name1, args, g)
                if res == "1-0": score2 += 1.0
                elif res == "0-1": score1 += 1.0
                else: score1 += 0.5; score2 += 0.5

            all_pgns.append(pgn)
            print(f"Current Match Standings: {name1} {score1:.1f} - {score2:.1f} {name2}")

        print("\n========================================================")
        print(f"🏆 FINAL MATCH RESULT: {name1} {score1:.1f} — {score2:.1f} {name2}")
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
