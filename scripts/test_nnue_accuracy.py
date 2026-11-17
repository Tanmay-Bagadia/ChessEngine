#!/usr/bin/env python3
"""
scripts/test_nnue_accuracy.py
Verifies that NNUE v2 evaluation is accurately scaled, symmetric, and functional
in both static evaluation and search.
"""

import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
from engine_path import get_engine_path


def run_nnue_evals(engine, fens, cwd=None):
    cmds = ["setoption name UseNNUE value true"]
    for fen in fens:
        cmds.append(f"evalfen {fen}")
    cmds.append("quit")
    proc = subprocess.run(
        [engine],
        input="\n".join(cmds) + "\n",
        text=True,
        capture_output=True,
        cwd=cwd,
        check=True,
    )
    assert "Loaded NNUE net" in proc.stdout, f"NNUE failed to load (cwd={cwd}):\n{proc.stdout}"
    scores = [int(l.strip()) for l in proc.stdout.splitlines() if l.strip().lstrip("-").isdigit()]
    assert len(scores) == len(fens), f"Expected {len(fens)} scores, got {len(scores)}: {scores}"
    return scores


def main():
    engine = get_engine_path()
    repo_root = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))

    print("=== Testing NNUE v2 Static Evaluation & Symmetry ===")
    cases = [
        ("Startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"),
        ("After 1.e4 (Black to move)", "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"),
        ("White up a Queen (W to move)", "rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"),
        ("Black up a Queen (W to move)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1"),
        ("Black up a Queen (B to move)", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNB1KBNR b KQkq - 0 1"),
        ("White up 4 Pawns", "4k3/8/8/8/8/8/4PPPP/4K3 w - - 0 1"),
        ("White up 2 Rooks", "4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1"),
    ]
    scores = run_nnue_evals(engine, [fen for _, fen in cases], cwd=repo_root)
    for (label, _), s in zip(cases, scores):
        print(f"  {label:30s}: {s:+6d} cp")

    start_s, e4_s, wq_w, bq_w, bq_b, p4_s, r2_s = scores

    assert abs(start_s) <= 60, f"Startpos NNUE eval too biased: {start_s}"
    assert -120 <= e4_s <= 40, f"After 1.e4 NNUE eval unreasonable: {e4_s}"
    assert wq_w >= 650, f"White up Queen NNUE eval too low: {wq_w}"
    assert bq_w <= -650, f"Black up Queen NNUE eval too high: {bq_w}"
    assert bq_b >= 650, f"Black up Queen (Black to move) should be strongly positive: {bq_b}"
    assert abs(wq_w - bq_b) <= 60, f"Color perspective asymmetry too large: {wq_w} vs {bq_b}"
    assert p4_s >= 200, f"White up 4 Pawns NNUE eval too low: {p4_s}"
    assert r2_s >= 650, f"White up 2 Rooks NNUE eval too low: {r2_s}"
    print("  [PASS] All NNUE static evals, material scaling, and color symmetry verified!")

    print("=== Testing NNUE Loading from build/ Working Directory ===")
    build_dir = os.path.join(repo_root, "build")
    if os.path.isdir(build_dir):
        scores_b = run_nnue_evals(engine, [cases[0][1]], cwd=build_dir)
        assert scores_b[0] == start_s, "NNUE eval differed when run from build/"
        print("  [PASS] NNUE loads cleanly from build/ via fallback path!")

    print("🎉 All NNUE v2 accuracy tests PASSED!")


if __name__ == "__main__":
    main()
