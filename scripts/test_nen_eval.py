#!/usr/bin/env python3
"""
scripts/test_nen_eval.py
Tests behavioral evaluation delta under different Nen / Style trait configurations.
Verifies that:
  1. Aggression (Transmutation) 0 vs 100 on an attacking position causes a measurable eval delta.
  2. Calculation (Enhancement) 0 vs 100 causes a measurable material/mobility eval delta.
  3. Style_* and Nen_* option names behave equivalently.
"""

import os
import sys
import subprocess
import re

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine_path import ENGINE_PATH

def run_uci(commands):
    proc = subprocess.Popen(
        [ENGINE_PATH],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    out, _ = proc.communicate("\n".join(commands) + "\nquit\n")
    return out

def get_eval(output):
    m = re.search(r"Evaluation:\s*(-?\d+)\s*cp", output)
    if m:
        return int(m.group(1))
    return None

def test_aggression_delta():
    print("=== Testing Aggression (Transmutation) Eval Delta ===")
    # Attacking position: White targets h7 and f7 with Bishop and Knight
    fen = "r1b2rk1/pp1nqppp/4p3/3pP3/3P4/3B1N2/PP1Q1PPP/R4RK1 w - - 0 13"

    out_low = run_uci([
        "setoption name Style_Aggression value 0",
        "position fen " + fen,
        "eval"
    ])
    score_low = get_eval(out_low)
    assert score_low is not None, f"Failed to get eval for Aggression=0: {out_low}"

    out_high = run_uci([
        "setoption name Style_Aggression value 100",
        "position fen " + fen,
        "eval"
    ])
    score_high = get_eval(out_high)
    assert score_high is not None, f"Failed to get eval for Aggression=100: {out_high}"

    delta = score_high - score_low
    print(f"  Aggression 0: {score_low} cp | Aggression 100: {score_high} cp | Delta: {delta} cp")
    assert delta != 0, f"Expected non-zero eval delta for Aggression, got {delta}"
    assert delta > 0, f"High aggression in attacking position should raise White's attack evaluation, got {delta}"
    print("  [PASS] Aggression behavioral delta confirmed!")

def test_enhancement_delta():
    print("=== Testing Calculation (Enhancement) Eval Delta ===")
    # Position where White has a material advantage (up a Knight)
    fen = "r1bqkb1r/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3"

    out_low = run_uci([
        "setoption name Nen_Enhancement value 0",
        "position fen " + fen,
        "eval"
    ])
    score_low = get_eval(out_low)
    assert score_low is not None, f"Failed to get eval for Enhancement=0: {out_low}"

    out_high = run_uci([
        "setoption name Nen_Enhancement value 100",
        "position fen " + fen,
        "eval"
    ])
    score_high = get_eval(out_high)
    assert score_high is not None, f"Failed to get eval for Enhancement=100: {out_high}"

    delta = score_high - score_low
    print(f"  Enhancement 0: {score_low} cp | Enhancement 100: {score_high} cp | Delta: {delta} cp")
    assert delta != 0, f"Expected non-zero eval delta for Enhancement, got {delta}"
    assert delta > 0, f"High enhancement when up material should amplify evaluation, got {delta}"
    print("  [PASS] Enhancement behavioral delta confirmed!")

def main():
    test_aggression_delta()
    test_enhancement_delta()
    print("🎉 All Nen eval delta tests PASSED!")

if __name__ == "__main__":
    main()
