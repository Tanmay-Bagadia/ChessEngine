#!/usr/bin/env python3
"""
nnue_trainer/train.py
Trains the Simple768 NNUE network (768 -> 256 -> 32 -> 32 -> 1) on:
  1. All prime-era Grandmaster PGN games in data/pgn/*.pgn (900 games)
  2. Material-imbalance & endgame augmentation positions (so the net scores
     large material advantages accurately alongside subtle GM middlegames)
  3. Color-flipped mirror symmetry for every position

Labels are generated using the engine's classical PeSTO + structure + mobility
evaluation blended with actual GM game outcomes (WDL), then quantized to NNv2.
"""

import glob
import math
import os
import random
import subprocess
import sys
import time
import chess
import chess.pgn
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import TensorDataset, DataLoader

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
sys.path.insert(0, SCRIPT_DIR)

from model import Simple768NNUE, NNUE_OUTPUT_SCALE
from dataset import extract_features_from_board


def find_engine_binary():
    for candidate in [
        os.environ.get("ENGINE_PATH", ""),
        os.path.join(REPO_ROOT, "build", "engine"),
        os.path.join(REPO_ROOT, "engine"),
    ]:
        if candidate and os.path.isfile(candidate):
            return candidate
    raise SystemExit("Error: build/engine binary not found. Build with: cmake -S . -B build && cmake --build build -j")


def batch_eval_fens(engine_path, fens):
    """Uses the C++ engine's classical PeSTO evaluation via `evalfen` in one fast pipe."""
    print(f"Labeling {len(fens)} positions using classical PeSTO engine ({engine_path})...")
    t0 = time.time()
    cmds = ["setoption name UseNNUE value false"]
    cmds.extend(f"evalfen {fen}" for fen in fens)
    cmds.append("quit")
    payload = "\n".join(cmds) + "\n"

    proc = subprocess.run(
        [engine_path],
        input=payload,
        text=True,
        capture_output=True,
        cwd=REPO_ROOT,
        check=True,
    )

    scores = []
    for line in proc.stdout.splitlines():
        s = line.strip()
        if not s:
            continue
        if s.startswith("info ") or s.startswith("id ") or s.startswith("option "):
            continue
        try:
            scores.append(int(s))
        except ValueError:
            continue

    if len(scores) != len(fens):
        raise RuntimeError(f"Expected {len(fens)} eval scores, got {len(scores)}")
    print(f"Labeled {len(scores)} positions in {time.time() - t0:.2f}s.")
    return scores


def load_gm_positions(pgn_dir, max_per_game=80):
    """Extracts quiet and middlegame/endgame positions + game WDL from prime-era GM PGNs."""
    pgn_files = sorted(glob.glob(os.path.join(pgn_dir, "*.pgn")))
    if not pgn_files:
        raise SystemExit(f"Error: No PGN files found in {pgn_dir}")

    fens = []
    wdls = []
    total_games = 0

    for pgn_path in pgn_files:
        with open(pgn_path, "r", encoding="utf-8", errors="replace") as f:
            while True:
                game = chess.pgn.read_game(f)
                if game is None:
                    break
                res_str = game.headers.get("Result", "*")
                if res_str == "1-0":
                    white_wdl = 1.0
                elif res_str == "0-1":
                    white_wdl = 0.0
                elif res_str == "1/2-1/2":
                    white_wdl = 0.5
                else:
                    white_wdl = 0.5

                total_games += 1
                board = game.board()
                ply = 0
                for move in game.mainline_moves():
                    board.push(move)
                    ply += 1
                    # Include opening moves occasionally and all middlegame/endgame plies
                    if ply < 4 and random.random() > 0.25:
                        continue
                    if ply > max_per_game + 6:
                        break
                    if board.is_game_over():
                        break
                    fens.append(board.fen())
                    wdl = white_wdl if board.turn == chess.WHITE else (1.0 - white_wdl)
                    wdls.append(wdl)

    print(f"Extracted {len(fens)} positions from {total_games} prime-era GM games across {len(pgn_files)} files.")
    return fens, wdls


def generate_augmented_positions(seed_fens, num_aug=25000):
    """
    Generates material-imbalance & endgame positions (piece drops, random walks,
    sparse endgames) so the NNUE learns accurate centipawn scaling across all
    material imbalances (+1 pawn up to +15 pawns).
    """
    random.seed(42)
    aug_fens = []

    # 1. Startpos and common openings
    base_board = chess.Board()
    aug_fens.append(base_board.fen())
    for uci in ["e2e4", "d2d4", "c2c4", "g1f3"]:
        b = chess.Board()
        b.push_uci(uci)
        aug_fens.append(b.fen())

    # 2. Piece-drop perturbations from real GM positions (teaches exact piece values!)
    sampled = random.sample(seed_fens, min(num_aug // 2, len(seed_fens)))
    for fen in sampled:
        b = chess.Board(fen)
        removable = [
            sq for sq in chess.SQUARES
            if b.piece_at(sq) is not None and b.piece_at(sq).piece_type != chess.KING
        ]
        if len(removable) >= 2:
            drop_count = random.choice([1, 1, 1, 2, 3])
            for sq in random.sample(removable, min(drop_count, len(removable))):
                b.remove_piece_at(sq)
            if not b.is_check() and b.is_valid():
                aug_fens.append(b.fen())

    # 3. Synthetic endgames (King + 1-5 pawns / rooks / minors / queens)
    pieces_pool = [chess.PAWN, chess.PAWN, chess.PAWN, chess.KNIGHT, chess.BISHOP, chess.ROOK, chess.QUEEN]
    while len(aug_fens) < num_aug:
        b = chess.Board.empty()
        wk = random.randint(0, 63)
        bk = random.randint(0, 63)
        if wk == bk or chess.square_distance(wk, bk) <= 1:
            continue
        b.set_piece_at(wk, chess.Piece(chess.KING, chess.WHITE))
        b.set_piece_at(bk, chess.Piece(chess.KING, chess.BLACK))

        n_white = random.randint(0, 5)
        n_black = random.randint(0, 5)
        occupied = {wk, bk}
        for color, count in [(chess.WHITE, n_white), (chess.BLACK, n_black)]:
            for _ in range(count):
                pt = random.choice(pieces_pool)
                sq = random.randint(8, 55) if pt == chess.PAWN else random.randint(0, 63)
                if sq not in occupied:
                    b.set_piece_at(sq, chess.Piece(pt, color))
                    occupied.add(sq)
        b.turn = random.choice([chess.WHITE, chess.BLACK])
        if not b.is_check() and b.is_valid():
            aug_fens.append(b.fen())

    print(f"Generated {len(aug_fens)} material-imbalance & endgame augmentation positions.")
    return aug_fens


def build_feature_tensors(fens, scores_cp, wdls, is_gm_mask):
    """
    Converts FENs to dense float32 tensors + color-flipped mirrors for exact symmetry.
    """
    n = len(fens)
    total = n * 2
    print(f"Building feature tensors for {total} positions (including color-flip mirrors)...")
    t0 = time.time()

    us_arr = np.zeros((total, 768), dtype=np.float32)
    them_arr = np.zeros((total, 768), dtype=np.float32)
    z_arr = np.zeros((total, 1), dtype=np.float32)
    prob_arr = np.zeros((total, 1), dtype=np.float32)

    for i in range(n):
        board = chess.Board(fens[i])
        cp = max(-2000.0, min(2000.0, float(scores_cp[i])))
        z = cp / NNUE_OUTPUT_SCALE
        eval_prob = 1.0 / (1.0 + math.exp(-z))
        # Blend classical evaluation with actual GM game result (WDL) for real GM games
        target_prob = (0.85 * eval_prob + 0.15 * wdls[i]) if is_gm_mask[i] else eval_prob

        u, t = extract_features_from_board(board)
        us_arr[2 * i] = u
        them_arr[2 * i] = t
        z_arr[2 * i, 0] = z
        prob_arr[2 * i, 0] = target_prob

        # Color-flipped mirror (identical side-to-move score & features guarantee symmetry)
        m_board = board.mirror()
        mu, mt = extract_features_from_board(m_board)
        us_arr[2 * i + 1] = mu
        them_arr[2 * i + 1] = mt
        z_arr[2 * i + 1, 0] = z
        prob_arr[2 * i + 1, 0] = target_prob

    print(f"Built {total} feature vectors in {time.time() - t0:.2f}s.")
    return (
        torch.from_numpy(us_arr),
        torch.from_numpy(them_arr),
        torch.from_numpy(z_arr),
        torch.from_numpy(prob_arr),
    )


def train_nnue(epochs=25, batch_size=512, lr=3e-3, export_path=None):
    if export_path is None:
        export_path = os.path.join(REPO_ROOT, "nnue.bin")

    if torch.backends.mps.is_available():
        device = torch.device("mps")
    elif torch.cuda.is_available():
        device = torch.device("cuda")
    else:
        device = torch.device("cpu")
    print(f"Training NNUE v2 on device: {device}")

    engine_path = find_engine_binary()
    pgn_dir = os.path.join(REPO_ROOT, "data", "pgn")

    gm_fens, gm_wdls = load_gm_positions(pgn_dir)
    aug_fens = generate_augmented_positions(gm_fens, num_aug=25000)

    all_fens = gm_fens + aug_fens
    all_wdls = gm_wdls + [0.5] * len(aug_fens)
    is_gm_mask = [True] * len(gm_fens) + [False] * len(aug_fens)

    scores_cp = batch_eval_fens(engine_path, all_fens)
    us_t, them_t, z_t, prob_t = build_feature_tensors(all_fens, scores_cp, all_wdls, is_gm_mask)

    # 95% train / 5% validation split
    total = us_t.shape[0]
    perm = torch.randperm(total, generator=torch.Generator().manual_seed(42))
    val_size = max(2000, total // 20)
    val_idx = perm[:val_size]
    train_idx = perm[val_size:]

    train_ds = TensorDataset(us_t[train_idx], them_t[train_idx], z_t[train_idx], prob_t[train_idx])
    val_ds = TensorDataset(us_t[val_idx], them_t[val_idx], z_t[val_idx], prob_t[val_idx])

    train_loader = DataLoader(train_ds, batch_size=batch_size, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=1024, shuffle=False)

    model = Simple768NNUE().to(device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-4)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs, eta_min=1e-4)
    mse = nn.MSELoss()
    huber = nn.SmoothL1Loss(beta=0.5)

    t_start = time.time()
    for epoch in range(epochs):
        model.train()
        train_loss = 0.0
        steps = 0
        for u, t, z_target, p_target in train_loader:
            u = u.to(device)
            t = t.to(device)
            z_target = z_target.to(device)
            p_target = p_target.to(device)

            optimizer.zero_grad()
            z_pred = model(u, t)
            p_pred = torch.sigmoid(z_pred)

            # Combined win-probability MSE + centipawn SmoothL1 (prevents sigmoid saturation)
            loss = mse(p_pred, p_target) + 0.15 * huber(z_pred, z_target)
            loss.backward()
            optimizer.step()
            model.clamp_weights()

            train_loss += loss.item()
            steps += 1

        scheduler.step()

        # Validation MAE in centipawns
        model.eval()
        val_mae_cp = 0.0
        val_count = 0
        with torch.no_grad():
            for u, t, z_target, _ in val_loader:
                u = u.to(device)
                t = t.to(device)
                z_target = z_target.to(device)
                z_pred = model(u, t)
                val_mae_cp += torch.abs((z_pred - z_target) * NNUE_OUTPUT_SCALE).sum().item()
                val_count += u.shape[0]

        val_mae_cp /= max(val_count, 1)
        if (epoch + 1) % 5 == 0 or epoch == 0 or epoch == epochs - 1:
            print(
                f"Epoch {epoch + 1:2d}/{epochs} | Train Loss: {train_loss / steps:.5f} "
                f"| Val MAE: {val_mae_cp:5.1f} cp | LR: {scheduler.get_last_lr()[0]:.5f}"
            )

    print(f"Training completed in {time.time() - t_start:.1f}s.")
    model.export_quantized(export_path)

    # Sanity-check PyTorch float predictions vs C++ quantized NNUE
    model.eval()
    test_fens = [
        ("Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"),
        ("After 1.e4 (Black to move)", "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"),
        ("White up a Queen", "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"),
        ("Black up a Queen", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1"),
        ("White up 4 Pawns", "4k3/8/8/8/8/8/4PPPP/4K3 w - - 0 1"),
        ("White up 2 Rooks", "4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1"),
    ]
    print("\n=== PyTorch Float vs C++ Quantized NNUE Verification ===")
    cmds = ["setoption name UseNNUE value true"]
    cmds.extend(f"evalfen {fen}" for _, fen in test_fens)
    cmds.append("quit")
    proc = subprocess.run([engine_path], input="\n".join(cmds) + "\n", text=True, capture_output=True, cwd=REPO_ROOT)
    cpp_scores = [int(l.strip()) for l in proc.stdout.splitlines() if l.strip().lstrip("-").isdigit()]

    for idx, (label, fen) in enumerate(test_fens):
        u, t = extract_features_from_board(chess.Board(fen))
        with torch.no_grad():
            py_cp = model(torch.from_numpy(u).unsqueeze(0).to(device),
                          torch.from_numpy(t).unsqueeze(0).to(device)).item() * NNUE_OUTPUT_SCALE
        cpp_cp = cpp_scores[idx] if idx < len(cpp_scores) else None
        diff = abs(py_cp - cpp_cp) if cpp_cp is not None else -1
        print(f"  {label:28s}: PyTorch={py_cp:+7.1f} cp | C++={cpp_cp:+6d} cp | Diff={diff:4.1f} cp")

    return model


if __name__ == "__main__":
    export_file = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO_ROOT, "nnue.bin")
    train_nnue(epochs=25, batch_size=512, lr=3e-3, export_path=export_file)
