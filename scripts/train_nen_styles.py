#!/usr/bin/env python3
"""
scripts/train_nen_styles.py
Derives Hunter x Hunter Nen archetypes and engine evaluation profiles
from Grandmaster PGN collections.

Analyzes 6 GM datasets (at least 20 games each) across 6 chess features:
  - Material Density (material) -> Enhancement
  - King Attack Intensity (attack) -> Transmutation
  - Pawn Structure & Chains (pawn) -> Conjuration
  - Asymmetric Imbalance (imbalance) -> Specialization
  - King Shelter & Prophylaxis (prophylaxis) -> Manipulation
  - Piece Mobility (mobility) -> Emission

Generates engine_src/nen_profiles.h with full provenance metadata.
Supports --check mode for CI verification.
"""

import os
import sys
import argparse
from datetime import datetime

try:
    import chess
    import chess.pgn
except ImportError:
    raise SystemExit(
        "Error: python-chess is not installed.\n"
        "Please install requirements with: pip install -r requirements.txt"
    )

GM_ARCHETYPES = {
    "Tal": ("tal.pgn", "Transmutation"),
    "Petrosian": ("petrosian.pgn", "Manipulation"),
    "Karpov": ("karpov.pgn", "Conjuration"),
    "Morphy": ("morphy.pgn", "Emission"),
    "Kasparov": ("kasparov.pgn", "Enhancement"),
    "Carlsen": ("carlsen.pgn", "Specialization"),
}

FEATURE_TO_AFFINITY = [
    ("material", "enhancement"),
    ("attack", "transmutation"),
    ("pawn", "conjuration"),
    ("imbalance", "specialization"),
    ("prophylaxis", "manipulation"),
    ("mobility", "emission"),
]

def analyze_pgn(file_path):
    if not os.path.exists(file_path):
        raise SystemExit(f"Error: PGN file not found: {file_path}")

    total_positions = 0
    total_games = 0
    king_attack_score = 0.0
    pawn_structure_score = 0.0
    mobility_score = 0.0
    prophylaxis_score = 0.0
    imbalance_score = 0.0
    material_greed_score = 0.0

    with open(file_path, "r", encoding="utf-8", errors="replace") as f:
        while True:
            try:
                game = chess.pgn.read_game(f)
            except Exception as e:
                break
            if game is None:
                break

            total_games += 1
            board = game.board()
            for move in game.mainline_moves():
                board.push(move)
                total_positions += 1
                color = board.turn
                enemy = not color
                enemy_king_sq = board.king(enemy)
                friendly_king_sq = board.king(color)

                # 1. King Attack Intensity
                if enemy_king_sq is not None:
                    k_file = chess.square_file(enemy_king_sq)
                    k_rank = chess.square_rank(enemy_king_sq)
                    attackers = 0
                    for sq in chess.SQUARES:
                        if abs(chess.square_file(sq) - k_file) <= 1 and abs(chess.square_rank(sq) - k_rank) <= 1:
                            if board.is_attacked_by(color, sq):
                                attackers += 1
                    king_attack_score += attackers

                # 2. Pawn Structure & Chains
                pawns = board.pieces(chess.PAWN, color)
                chains = 0
                for p in pawns:
                    if board.is_attacked_by(color, p):
                        chains += 1
                pawn_structure_score += chains

                # 3. Mobility
                mobility_score += board.legal_moves.count()

                # 4. Prophylaxis & Shelter
                if friendly_king_sq is not None:
                    f_file = chess.square_file(friendly_king_sq)
                    f_rank = chess.square_rank(friendly_king_sq)
                    shield = 0
                    for sq in pawns:
                        if abs(chess.square_file(sq) - f_file) <= 1 and abs(chess.square_rank(sq) - f_rank) <= 2:
                            shield += 1
                    prophylaxis_score += shield

                # 5. Imbalance & Asymmetry
                b = len(board.pieces(chess.BISHOP, color))
                n = len(board.pieces(chess.KNIGHT, color))
                if b != n:
                    imbalance_score += 1

                # 6. Material Density
                mat = len(board.pieces(chess.QUEEN, color)) * 9 + len(board.pieces(chess.ROOK, color)) * 5
                material_greed_score += mat

    if total_games == 0 or total_positions == 0:
        raise SystemExit(f"Error: No valid games or plies found in {file_path}")

    return {
        "games": total_games,
        "positions": total_positions,
        "attack": king_attack_score / total_positions,
        "pawn": pawn_structure_score / total_positions,
        "mobility": mobility_score / total_positions,
        "prophylaxis": prophylaxis_score / total_positions,
        "imbalance": imbalance_score / total_positions,
        "material": material_greed_score / total_positions,
    }

def generate_header(check_mode=False):
    base_dir = os.path.dirname(os.path.abspath(__file__))
    pgn_dir = os.path.join(base_dir, "..", "data", "pgn")
    header_path = os.path.join(base_dir, "..", "engine_src", "nen_profiles.h")

    results = {}
    for gm, (fname, archetype) in GM_ARCHETYPES.items():
        fpath = os.path.join(pgn_dir, fname)
        data = analyze_pgn(fpath)
        if data["games"] < 20:
            print(f"Warning: {gm} dataset has {data['games']} games (expected >= 20)")
        results[gm] = data

    # Compute min-max for each feature
    features = [f[0] for f in FEATURE_TO_AFFINITY]
    f_min = {f: min(results[g][f] for g in GM_ARCHETYPES) for f in features}
    f_max = {f: max(results[g][f] for g in GM_ARCHETYPES) for f in features}

    # Derive profiles
    profiles = {}
    for gm in GM_ARCHETYPES:
        prof = {}
        for feat, aff in FEATURE_TO_AFFINITY:
            denom = f_max[feat] - f_min[feat]
            norm = (results[gm][feat] - f_min[feat]) / denom if denom > 0 else 0.5
            # Linear blend: 25..95 mapping, clamped 0..100
            val = int(round(25.0 + 70.0 * norm))
            val = max(0, min(100, val))
            prof[aff] = val
        profiles[gm] = prof

    # Construct provenance block
    lines = [
        "#pragma once",
        "",
        "// ============================================================================",
        "// Auto-generated by scripts/train_nen_styles.py from Grandmaster PGN Datasets",
        "// Maps Hunter x Hunter Nen Affinities to Engine Evaluation Multipliers",
        "// Reference: docs/nen_system.md and README.md",
        "// ============================================================================",
        "// Provenance & Calibration Metadata:",
        "// Mapping Formula: norm = (feat - min) / (max - min); affinity = round(25 + 70 * norm)",
        "// Datasets Analyzed:",
    ]
    for gm, (fname, arch) in GM_ARCHETYPES.items():
        d = results[gm]
        lines.append(f"//   - {gm:10} ({arch:14}): {d['games']:2} games, {d['positions']:5} plies")

    lines.append("// Measured Feature Bounds:")
    for feat in features:
        lines.append(f"//   - {feat:12}: min={f_min[feat]:.4f}, max={f_max[feat]:.4f}")

    lines.append("// ============================================================================")
    lines.append("")
    lines.append("struct NenProfile {")
    lines.append("    int enhancement = 50;     // 0 - 100: Material calculation, tactical precision")
    lines.append("    int transmutation = 50;   // 0 - 100: King attacks, dynamic sacrifices, initiative")
    lines.append("    int conjuration = 50;     // 0 - 100: Pawn structure integrity, outposts, fortresses")
    lines.append("    int specialization = 50;  // 0 - 100: Asymmetric imbalances, unorthodox maneuvers")
    lines.append("    int manipulation = 50;    // 0 - 100: Prophylaxis, king shelter, suffocating mobility")
    lines.append("    int emission = 50;        // 0 - 100: Long-range pieces, open files, piece mobility")
    lines.append("};")
    lines.append("")
    lines.append("// Calibrated Grandmaster Archetypes (Derived from Data)")
    lines.append("inline const NenProfile BALANCED_PROFILE   = {50, 50, 50, 50, 50, 50};")

    for gm in ["Tal", "Petrosian", "Karpov", "Morphy", "Kasparov", "Carlsen"]:
        p = profiles[gm]
        lines.append(
            f"inline const NenProfile {gm.upper()}_PROFILE = "
            f"{{{p['enhancement']}, {p['transmutation']}, {p['conjuration']}, "
            f"{p['specialization']}, {p['manipulation']}, {p['emission']}}};"
        )
    lines.append("")

    header_text = "\n".join(lines)

    if check_mode:
        if not os.path.exists(header_path):
            print(f"FAIL: {header_path} does not exist.")
            sys.exit(1)
        with open(header_path, "r", encoding="utf-8") as f:
            existing = f.read()
        if existing.strip() != header_text.strip():
            print(f"FAIL: {header_path} differs from trained data.")
            print("Run 'python3 scripts/train_nen_styles.py' to update.")
            sys.exit(1)
        print("OK: Committed nen_profiles.h matches data pipeline.")
        sys.exit(0)
    else:
        with open(header_path, "w", encoding="utf-8") as f:
            f.write(header_text)
        print(f"Successfully generated {header_path}")

def main():
    parser = argparse.ArgumentParser(description="Train Nen evaluation profiles from PGNs")
    parser.add_argument("--check", action="store_true", help="Check committed header without writing")
    args = parser.parse_args()
    generate_header(check_mode=args.check)

if __name__ == "__main__":
    main()
