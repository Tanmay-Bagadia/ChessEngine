#!/usr/bin/env python3
"""
scripts/test_uci_nen.py
Verifies UCI advertising and support for both Style_* and Nen_* option names.
"""

import os
import sys
import subprocess

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine_path import ENGINE_PATH

def test_uci_protocol():
    print("=== Testing UCI Protocol & Option Advertising ===")
    proc = subprocess.Popen([ENGINE_PATH], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    out, _ = proc.communicate("uci\nquit\n")

    # 1. Verify engine rename
    assert "id name Chess Engine" in out, f"Engine should be named 'Chess Engine', got:\n{out}"
    assert "TanmayChess" not in out, "Old name 'TanmayChess' should not appear in UCI id name"

    # 2. Verify all Style options are advertised
    style_traits = ["Calculation", "Aggression", "Structure", "Creativity", "Prophylaxis", "Activity"]
    for trait in style_traits:
        opt_str = f"option name Style_{trait} type spin default 50 min 0 max 100"
        assert opt_str in out, f"Missing UCI option: {opt_str}"
    assert "option name Style_Preset type combo" in out, "Missing Style_Preset combo option"

    # 3. Verify all Nen options are advertised (contract: engine advertises both)
    nen_traits = ["Enhancement", "Transmutation", "Conjuration", "Specialization", "Manipulation", "Emission"]
    for trait in nen_traits:
        opt_str = f"option name Nen_{trait} type spin default 50 min 0 max 100"
        assert opt_str in out, f"Missing UCI option: {opt_str}"
    assert "option name Nen_Preset type combo" in out, "Missing Nen_Preset combo option"

    print("  [PASS] UCI protocol, Style_* and Nen_* options verified!")

def test_uci_presets():
    print("=== Testing Style and Nen Presets via UCI ===")
    presets = ["Tal", "Petrosian", "Karpov", "Morphy", "Kasparov", "Carlsen", "Balanced"]
    commands = []
    for p in presets:
        commands.append(f"setoption name Style_Preset value {p}")
        commands.append("position startpos")
        commands.append("eval")
        commands.append(f"setoption name Nen_Preset value {p}")
        commands.append("position startpos")
        commands.append("eval")
    commands.append("quit")

    proc = subprocess.Popen([ENGINE_PATH], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    out, _ = proc.communicate("\n".join(commands) + "\n")
    eval_count = out.count("Evaluation:")
    assert eval_count == len(presets) * 2, f"Expected {len(presets)*2} evaluations, got {eval_count}:\n{out}"

    print(f"  [PASS] All {len(presets)*2} presets verified successfully in a single session!")

if __name__ == "__main__":
    test_uci_protocol()
    test_uci_presets()
    print("🎉 All UCI Nen tests PASSED!")
