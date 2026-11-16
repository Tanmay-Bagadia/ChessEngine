#!/usr/bin/env python3
"""
scripts/run_all_tests.py
Master test runner executing the complete test suite:
  1. scripts/train_nen_styles.py --check (Header provenance CI gate)
  2. scripts/test_engine.py (Perft benchmark suite, tactics, time limits, stop command)
  3. scripts/test_nen_eval.py (Nen aggression and enhancement eval deltas)
  4. scripts/test_uci_nen.py (UCI protocol, Style_* and Nen_* option advertising)
  5. scripts/test_game_flow.py (Game flow 5x regression test against move duplication)
  6. scripts/test_gui_server.py (GUI server API endpoints & state persistence)

Exits non-zero immediately on any test failure. Passes with $ENGINE_PATH unset.
"""

import os
import sys
import subprocess

def run_test_module(name, cmd):
    print(f"\n========================================================")
    print(f"🚀 RUNNING: {name}")
    print(f"Command: {' '.join(cmd)}")
    print(f"========================================================")
    proc = subprocess.run(cmd)
    if proc.returncode != 0:
        print(f"\n❌ FAILED: {name} exited with code {proc.returncode}")
        sys.exit(proc.returncode)
    print(f"✅ PASSED: {name}")

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))

    # Ensure ENGINE_PATH is unset if user wants to test default resolution
    # (or preserve if set, but verified to work without it)
    tests = [
        ("Nen Training CI Check", [sys.executable, os.path.join(base_dir, "train_nen_styles.py"), "--check"]),
        ("Engine Core & Perft Suite", [sys.executable, os.path.join(base_dir, "test_engine.py")]),
        ("Nen Evaluation Behavioral Deltas", [sys.executable, os.path.join(base_dir, "test_nen_eval.py")]),
        ("UCI Protocol & Nen/Style Options", [sys.executable, os.path.join(base_dir, "test_uci_nen.py")]),
        ("Game Flow Multi-Run Duplication Test", [sys.executable, os.path.join(base_dir, "test_game_flow.py")]),
        ("GUI Server REST & State Endpoints", [sys.executable, os.path.join(base_dir, "test_gui_server.py")]),
    ]

    for name, cmd in tests:
        run_test_module(name, cmd)

    print("\n" + "="*56)
    print("🏆 ALL TEST SUITES PASSED END-TO-END! 🏆")
    print("="*56)

if __name__ == "__main__":
    main()
