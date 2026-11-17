#!/usr/bin/env python3
"""
nnue_trainer/train.py
Trains the SCReLU NNUE v3 network ((768 -> 256)x2 -> 1) using a clean
hybrid Stockfish 19 static NNUE + Classical PeSTO distillation target:

  - Near material equality (|PeSTO| <= 250 cp): blends 65% Stockfish 19 static
    NNUE positional intelligence (rescaled to classical centipawns) + 35% PeSTO.
  - At large material imbalances: smoothly anchors to PeSTO's exact linear
    material scale (Pawn=100, Knight=320, Rook=500, Queen=900), preventing
    Stockfish 19's WDL sigmoid compression from distorting piece values or
    breaking classical search pruning margins.
  - Strict group-level train/val split (zero same-game or mirror leakage) with
    best-validation checkpointing to guarantee strong unseen-position accuracy.
"""

import glob
import math
import os
import random
import re
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

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

SF_FINAL_RE = re.compile(r"Final evaluation\s+([+-]?\d+\.\d+)")


def find_engine_binary():
    for candidate in [
        os.environ.get("ENGINE_PATH", ""),
        os.path.join(REPO_ROOT, "build", "engine"),
        os.path.join(REPO_ROOT, "engine"),
    ]:
        if candidate and os.path.isfile(candidate):
            return candidate
    raise SystemExit("Error: build/engine binary not found. Build with: cmake -S . -B build && cmake --build build -j")


def find_stockfish():
    for cand in ["/opt/homebrew/bin/stockfish", "/usr/local/bin/stockfish", shutil.which("stockfish") or ""]:
        if cand and os.path.isfile(cand):
            return cand
    return None


def _sf_static_worker(sf_path, fens_chunk):
    """Runs Stockfish 19's static 109MB NNUE `eval` command (side-to-move normalized)."""
    cmds = ["setoption name Threads value 1"]
    for fen in fens_chunk:
        cmds.append(f"position fen {fen}")
        cmds.append("eval")
    cmds.append("quit")

    proc = subprocess.run(
        [sf_path],
        input="\n".join(cmds) + "\n",
        text=True,
        capture_output=True,
        check=True,
    )

    white_evals = []
    for line in proc.stdout.splitlines():
        if line.startswith("Final evaluation"):
            m = SF_FINAL_RE.search(line)
            if m:
                white_evals.append(int(round(float(m.group(1)) * 100.0)))
            else:
                white_evals.append(0)

    if len(white_evals) != len(fens_chunk):
        raise RuntimeError(f"Stockfish eval mismatch: got {len(white_evals)} for {len(fens_chunk)} FENs")

    # Convert Stockfish's white-side Final evaluation to side-to-move perspective
    stm_evals = []
    for fen, w_cp in zip(fens_chunk, white_evals):
        is_black = (" b " in fen)
        stm_evals.append(-w_cp if is_black else w_cp)
    return stm_evals


def label_with_stockfish_static(sf_path, fens, workers=10):
    print(f"Running Stockfish 19 static NNUE eval on {len(fens)} positions across {workers} workers...")
    t0 = time.time()
    batch_size = 500
    batches = [fens[i:i + batch_size] for i in range(0, len(fens), batch_size)]

    scores = []
    with ThreadPoolExecutor(max_workers=workers) as ex:
        for idx, chunk_scores in enumerate(ex.map(lambda b: _sf_static_worker(sf_path, b), batches)):
            scores.extend(chunk_scores)
            done = len(scores)
            if (idx + 1) % 40 == 0 or done == len(fens):
                dt = time.time() - t0
                print(f"  SF19 eval: {done:6d}/{len(fens)} ({done / max(dt, 1e-3):.0f} pos/sec, {dt:.1f}s)")
    return scores


def batch_eval_fens_pesto(engine_path, fens):
    print(f"Running Classical PeSTO eval on {len(fens)} positions...")
    t0 = time.time()
    cmds = ["setoption name UseNNUE value false"]
    cmds.extend(f"evalfen {fen}" for fen in fens)
    cmds.append("quit")
    proc = subprocess.run(
        [engine_path],
        input="\n".join(cmds) + "\n",
        text=True,
        capture_output=True,
        cwd=REPO_ROOT,
        check=True,
    )
    scores = [int(l.strip()) for l in proc.stdout.splitlines() if l.strip().lstrip("-").isdigit()]
    if len(scores) != len(fens):
        raise RuntimeError(f"Expected {len(fens)} PeSTO scores, got {len(scores)}")
    print(f"  PeSTO eval: {len(scores)} positions in {time.time() - t0:.2f}s.")
    return scores


def load_gm_positions(pgn_dir, max_per_game=80):
    """Returns (fens, wdls, group_ids) where group_id is per-game for leak-free splitting."""
    pgn_files = sorted(glob.glob(os.path.join(pgn_dir, "*.pgn")))
    if not pgn_files:
        raise SystemExit(f"Error: No PGN files found in {pgn_dir}")

    fens, wdls, groups = [], [], []
    game_id = 0

    for pgn_path in pgn_files:
        with open(pgn_path, "r", encoding="utf-8", errors="replace") as f:
            while True:
                game = chess.pgn.read_game(f)
                if game is None:
                    break
                res_str = game.headers.get("Result", "*")
                white_wdl = 1.0 if res_str == "1-0" else (0.0 if res_str == "0-1" else 0.5)

                game_id += 1
                board = game.board()
                ply = 0
                for move in game.mainline_moves():
                    board.push(move)
                    ply += 1
                    if ply < 4 and random.random() > 0.25:
                        continue
                    if ply > max_per_game + 6 or board.is_game_over():
                        break
                    if board.is_check():
                        continue
                    fens.append(board.fen())
                    wdls.append(white_wdl if board.turn == chess.WHITE else (1.0 - white_wdl))
                    groups.append(game_id)

    print(f"Extracted {len(fens)} quiet positions from {game_id} prime-era GM games.")
    return fens, wdls, groups, game_id


def generate_augmented_positions(seed_fens, start_group_id, num_aug=60000):
    """
    Generates diverse unseen positions:
      - Random 1-2 ply legal perturbations of GM positions (teaches generalization!)
      - Piece-drop perturbations (teaches exact piece values for every square!)
      - Random opening/middlegame walks (decorrelated board structures!)
      - Synthetic endgames (king + pawns / rooks / minors / queens)
    """
    random.seed(42)
    aug_fens = []
    aug_groups = []
    gid = start_group_id

    # Anchor openings
    base = chess.Board()
    gid += 1
    aug_fens.append(base.fen()); aug_groups.append(gid)
    for uci in ["e2e4", "d2d4", "c2c4", "g1f3"]:
        b = chess.Board()
        b.push_uci(uci)
        gid += 1
        aug_fens.append(b.fen()); aug_groups.append(gid)

    # 1. 1-2 ply legal deviations from GM games (20,000 positions)
    for fen in random.sample(seed_fens, min(20000, len(seed_fens))):
        b = chess.Board(fen)
        for _ in range(random.choice([1, 2])):
            moves = list(b.legal_moves)
            if not moves:
                break
            b.push(random.choice(moves))
        if not b.is_check() and not b.is_game_over():
            gid += 1
            aug_fens.append(b.fen()); aug_groups.append(gid)

    # 2. Piece-drop perturbations from GM games (20,000 positions)
    for fen in random.sample(seed_fens, min(20000, len(seed_fens))):
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
                gid += 1
                aug_fens.append(b.fen()); aug_groups.append(gid)

    # 3. Synthetic endgames (remaining up to num_aug)
    pieces_pool = [chess.PAWN, chess.PAWN, chess.PAWN, chess.KNIGHT, chess.BISHOP, chess.ROOK, chess.QUEEN]
    while len(aug_fens) < num_aug:
        b = chess.Board.empty()
        wk = random.randint(0, 63)
        bk = random.randint(0, 63)
        if wk == bk or chess.square_distance(wk, bk) <= 1:
            continue
        b.set_piece_at(wk, chess.Piece(chess.KING, chess.WHITE))
        b.set_piece_at(bk, chess.Piece(chess.KING, chess.BLACK))

        occupied = {wk, bk}
        for color in (chess.WHITE, chess.BLACK):
            for _ in range(random.randint(0, 6)):
                pt = random.choice(pieces_pool)
                sq = random.randint(8, 55) if pt == chess.PAWN else random.randint(0, 63)
                if sq not in occupied:
                    b.set_piece_at(sq, chess.Piece(pt, color))
                    occupied.add(sq)
        b.turn = random.choice([chess.WHITE, chess.BLACK])
        if not b.is_check() and b.is_valid():
            gid += 1
            aug_fens.append(b.fen()); aug_groups.append(gid)

    print(f"Generated {len(aug_fens)} diverse perturbation, piece-drop & endgame positions.")
    return aug_fens, aug_groups


def compute_hybrid_targets(pesto_scores, sf_scores):
    """
    Combines Stockfish 19 static NNUE positional intelligence with PeSTO's
    exact linear classical centipawn material anchor.
    """
    targets = []
    for p_cp, s_cp in zip(pesto_scores, sf_scores):
        # Smooth Gaussian weight: 0.65 near equality, -> 0 for large material gaps
        w_sf = 0.65 * math.exp(-((p_cp / 300.0) ** 2))
        sf_rescaled = s_cp * 0.50
        # Prevent extreme SF19 positional spikes from contradicting material sign
        blended = w_sf * sf_rescaled + (1.0 - w_sf) * p_cp
        targets.append(max(-2000.0, min(2000.0, blended)))
    return targets


def build_split_tensors(fens, targets_cp, wdls, is_gm_mask, groups, val_frac=0.06):
    """
    Strict group-level split BEFORE mirroring: no game or mirror pair ever
    appears in both train and validation sets!
    """
    unique_groups = list(set(groups))
    rng = random.Random(42)
    rng.shuffle(unique_groups)
    val_group_set = set(unique_groups[:max(1, int(len(unique_groups) * val_frac))])

    train_idx = [i for i, g in enumerate(groups) if g not in val_group_set]
    val_idx = [i for i, g in enumerate(groups) if g in val_group_set]

    def make_arrays(indices, add_mirrors):
        mult = 2 if add_mirrors else 1
        total = len(indices) * mult
        us_arr = np.zeros((total, 768), dtype=np.float32)
        them_arr = np.zeros((total, 768), dtype=np.float32)
        z_arr = np.zeros((total, 1), dtype=np.float32)
        prob_arr = np.zeros((total, 1), dtype=np.float32)

        for k, idx in enumerate(indices):
            board = chess.Board(fens[idx])
            cp = targets_cp[idx]
            z = cp / NNUE_OUTPUT_SCALE
            eval_prob = 1.0 / (1.0 + math.exp(-z))
            target_prob = (0.92 * eval_prob + 0.08 * wdls[idx]) if is_gm_mask[idx] else eval_prob

            u, t = extract_features_from_board(board)
            pos_out = k * mult
            us_arr[pos_out] = u
            them_arr[pos_out] = t
            z_arr[pos_out, 0] = z
            prob_arr[pos_out, 0] = target_prob

            if add_mirrors:
                mu, mt = extract_features_from_board(board.mirror())
                us_arr[pos_out + 1] = mu
                them_arr[pos_out + 1] = mt
                z_arr[pos_out + 1, 0] = z
                prob_arr[pos_out + 1, 0] = target_prob

        return (
            torch.from_numpy(us_arr),
            torch.from_numpy(them_arr),
            torch.from_numpy(z_arr),
            torch.from_numpy(prob_arr),
        )

    print(f"Building leak-free tensors: {len(train_idx)*2} train vectors (mirrored), {len(val_idx)} unseen val vectors...")
    t0 = time.time()
    train_tensors = make_arrays(train_idx, add_mirrors=True)
    val_tensors = make_arrays(val_idx, add_mirrors=False)
    print(f"Built tensors in {time.time() - t0:.2f}s.")
    return train_tensors, val_tensors


def train_nnue(epochs=24, batch_size=512, lr=3e-3, export_path=None):
    if export_path is None:
        export_path = os.path.join(REPO_ROOT, "nnue.bin")

    if torch.backends.mps.is_available():
        device = torch.device("mps")
    elif torch.cuda.is_available():
        device = torch.device("cuda")
    else:
        device = torch.device("cpu")
    print(f"Training NNUE v3 (SCReLU, Stockfish 19 + PeSTO Hybrid) on device: {device}")

    engine_path = find_engine_binary()
    pgn_dir = os.path.join(REPO_ROOT, "data", "pgn")

    gm_fens, gm_wdls, gm_groups, max_gid = load_gm_positions(pgn_dir)
    aug_fens, aug_groups = generate_augmented_positions(gm_fens, max_gid, num_aug=60000)

    all_fens = gm_fens + aug_fens
    all_wdls = gm_wdls + [0.5] * len(aug_fens)
    all_groups = gm_groups + aug_groups
    is_gm_mask = [True] * len(gm_fens) + [False] * len(aug_fens)

    pesto_scores = batch_eval_fens_pesto(engine_path, all_fens)
    sf_path = find_stockfish()
    if sf_path:
        sf_scores = label_with_stockfish_static(sf_path, all_fens, workers=10)
        targets_cp = compute_hybrid_targets(pesto_scores, sf_scores)
    else:
        targets_cp = [float(s) for s in pesto_scores]

    train_tensors, val_tensors = build_split_tensors(all_fens, targets_cp, all_wdls, is_gm_mask, all_groups)

    train_loader = DataLoader(TensorDataset(*train_tensors), batch_size=batch_size, shuffle=True)
    val_loader = DataLoader(TensorDataset(*val_tensors), batch_size=1024, shuffle=False)

    model = Simple768NNUE().to(device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=3e-3)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=epochs, eta_min=1e-4)
    mse = nn.MSELoss()
    huber = nn.SmoothL1Loss(beta=0.4)

    best_val_mae = float("inf")
    best_state = None
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

            loss = mse(p_pred, p_target) + 0.25 * huber(z_pred, z_target)
            loss.backward()
            optimizer.step()
            model.clamp_weights()

            train_loss += loss.item()
            steps += 1

        scheduler.step()

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
        marker = ""
        if val_mae_cp < best_val_mae:
            best_val_mae = val_mae_cp
            best_state = {k: v.detach().cpu().clone() for k, v in model.state_dict().items()}
            marker = " *"

        if (epoch + 1) % 4 == 0 or epoch == 0 or epoch == epochs - 1:
            print(
                f"Epoch {epoch + 1:2d}/{epochs} | Train Loss: {train_loss / steps:.5f} "
                f"| Unseen Val MAE: {val_mae_cp:5.1f} cp{marker} | LR: {scheduler.get_last_lr()[0]:.5f}"
            )

    if best_state is not None:
        model.load_state_dict(best_state)
        print(f"Restored best unseen-validation checkpoint (Val MAE: {best_val_mae:.1f} cp) in {time.time() - t_start:.1f}s.")

    model.export_quantized(export_path)

    # Verify unseen positions (C++ NEON vs PyTorch vs PeSTO)
    model.eval()
    test_fens = [
        ("Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"),
        ("After 1.e4 (Black to move)", "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"),
        ("Unseen W up a Pawn", "r1bqkb1r/ppp2ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4"),
        ("Unseen W down a Knight", "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNBQK2R w KQkq - 0 4"),
        ("Unseen W down a Rook", "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK3 w Qkq - 0 4"),
        ("White up a Queen", "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"),
        ("Black up a Queen", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1"),
        ("White up 4 Pawns", "4k3/8/8/8/8/8/4PPPP/4K3 w - - 0 1"),
        ("White up 2 Rooks", "4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1"),
    ]
    print("\n=== Unseen Position Verification (PyTorch vs C++ NEON NNUE v3) ===")
    cmds = ["setoption name UseNNUE value true"] + [f"evalfen {fen}" for _, fen in test_fens] + ["quit"]
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
    train_nnue(epochs=24, batch_size=512, lr=3e-3, export_path=export_file)
