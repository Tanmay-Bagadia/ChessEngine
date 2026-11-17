#!/usr/bin/env python3
"""
scripts/train_nen_styles.py
Derives calibrated Grandmaster playstyle profiles from prime-era PGN collections.

Analyzes 6 prime-era GM datasets (150 classical games each = 900 games total)
specifically from each Grandmaster's own perspective across 6 chess features:
  - Material Density / Piece Retention (material) -> Calculation
  - King Attack Intensity (attack)                -> Aggression
  - Pawn Structure & Chains (pawn)                -> Structure
  - Asymmetric Imbalance (imbalance)              -> Creativity
  - King Shelter & Defensive Control (defense)    -> Defense
  - Piece Mobility & Scope (mobility)             -> Activity

Generates engine_src/nen_profiles.h with full provenance metadata.
Supports --check mode for CI verification.
"""

import os
import sys
import argparse

try:
    import chess
    import chess.pgn
except ImportError:
    raise SystemExit(
        "Error: python-chess is not installed.\n"
        "Please install requirements with: pip install -r requirements.txt"
    )

GM_ARCHETYPES = {
    "Tal":       ("tal.pgn",       "Aggression",  "1957-1961", "tal"),
    "Petrosian": ("petrosian.pgn", "Defense",     "1962-1969", "petrosian"),
    "Karpov":    ("karpov.pgn",    "Structure",   "1974-1985", "karpov"),
    "Morphy":    ("morphy.pgn",    "Activity",    "1857-1859", "morphy"),
    "Kasparov":  ("kasparov.pgn",  "Calculation", "1985-2000", "kasparov"),
    "Carlsen":   ("carlsen.pgn",   "Creativity",  "2013-2021", "carlsen"),
}

FEATURE_TO_TRAIT = [
    ("material",  "enhancement",    "Calculation"),
    ("attack",    "transmutation",  "Aggression"),
    ("pawn",      "conjuration",    "Structure"),
    ("imbalance", "specialization", "Creativity"),
    ("defense",   "manipulation",   "Defense"),
    ("mobility",  "emission",       "Activity"),
]

# Precomputed king-zone bitboard masks for fast analysis
KING_ZONES = {}
for _sq in chess.SQUARES:
    _kf, _kr = chess.square_file(_sq), chess.square_rank(_sq)
    KING_ZONES[_sq] = [
        s for s in chess.SQUARES
        if abs(chess.square_file(s) - _kf) <= 1 and abs(chess.square_rank(s) - _kr) <= 1
    ]


def analyze_pgn(file_path, match_str):
    if not os.path.exists(file_path):
        raise SystemExit(f"Error: PGN file not found: {file_path}")

    total_positions = 0
    middlegame_positions = 0
    total_games = 0
    king_attack_score = 0.0
    pawn_structure_score = 0.0
    mobility_score = 0.0
    defense_score = 0.0
    imbalance_score = 0.0
    calculation_score = 0.0

    with open(file_path, "r", encoding="utf-8", errors="replace") as f:
        while True:
            try:
                game = chess.pgn.read_game(f)
            except Exception:
                break
            if game is None:
                break

            w_name = game.headers.get("White", "").lower()
            b_name = game.headers.get("Black", "").lower()
            if match_str in w_name:
                gm_color = chess.WHITE
            elif match_str in b_name:
                gm_color = chess.BLACK
            else:
                continue

            total_games += 1
            board = game.board()
            enemy = not gm_color
            ply = 0

            for move in game.mainline_moves():
                mover = board.turn
                is_forcing = board.is_capture(move) or board.gives_check(move)
                board.push(move)
                ply += 1

                # Skip opening book moves (plies 1-10) and only evaluate after GM's own moves
                if ply <= 10 or mover != gm_color:
                    continue

                total_positions += 1
                enemy_king_sq = board.king(enemy)
                friendly_king_sq = board.king(gm_color)

                pawns = board.pieces(chess.PAWN, gm_color)
                non_pawn_pieces = (
                    len(board.pieces(chess.KNIGHT, gm_color))
                    + len(board.pieces(chess.BISHOP, gm_color))
                    + len(board.pieces(chess.ROOK, gm_color))
                    + len(board.pieces(chess.QUEEN, gm_color))
                )
                enemy_queens = len(board.pieces(chess.QUEEN, enemy))

                # 1. Aggression: Middlegame King Attack Intensity (GM pieces attacking enemy king zone while queens are on board)
                if enemy_king_sq is not None and enemy_queens > 0:
                    attackers = 0
                    for sq in KING_ZONES[enemy_king_sq]:
                        if board.is_attacked_by(gm_color, sq):
                            attackers += 1
                    king_attack_score += attackers

                # 2. Structure: Pawn Chain Ratio (fraction of pawns solidly defended)
                if len(pawns) > 0:
                    chains = sum(1 for p in pawns if board.is_attacked_by(gm_color, p))
                    pawn_structure_score += chains / len(pawns)

                # 3. Activity: Average Mobility per Non-Pawn Piece
                if non_pawn_pieces > 0:
                    piece_attacks = 0
                    for pt in (chess.KNIGHT, chess.BISHOP, chess.ROOK, chess.QUEEN):
                        for sq in board.pieces(pt, gm_color):
                            piece_attacks += len(board.attacks(sq))
                    mobility_score += piece_attacks / non_pawn_pieces

                # 4. Defense: King Shelter & Safety in Middlegame (when opponent still has a queen)
                if friendly_king_sq is not None and enemy_queens > 0:
                    middlegame_positions += 1
                    f_file = chess.square_file(friendly_king_sq)
                    f_rank = chess.square_rank(friendly_king_sq)
                    shield = sum(
                        1 for sq in pawns
                        if abs(chess.square_file(sq) - f_file) <= 1 and abs(chess.square_rank(sq) - f_rank) <= 2
                    )
                    enemy_pressure = sum(1 for sq in KING_ZONES[friendly_king_sq] if board.is_attacked_by(enemy, sq))
                    defense_score += (shield - 0.5 * enemy_pressure)

                # 5. Creativity: Asymmetric Piece Imbalance (unequal bishops/knights/rooks/queens)
                wb = len(board.pieces(chess.BISHOP, gm_color))
                wn = len(board.pieces(chess.KNIGHT, gm_color))
                wr = len(board.pieces(chess.ROOK, gm_color))
                eb = len(board.pieces(chess.BISHOP, enemy))
                en = len(board.pieces(chess.KNIGHT, enemy))
                er = len(board.pieces(chess.ROOK, enemy))
                if (wb != eb) or (wn != en) or (wr != er):
                    imbalance_score += 1

                # 6. Calculation: Tactical / Forcing Move Density (captures, checks, and attacked enemy pieces)
                tactical_val = 1.0 if is_forcing else 0.0
                if board.is_check():
                    tactical_val += 0.5
                calculation_score += tactical_val

    if total_games == 0 or total_positions == 0:
        raise SystemExit(f"Error: No valid games or plies found in {file_path}")

    return {
        "games": total_games,
        "positions": total_positions,
        "attack": king_attack_score / max(middlegame_positions, 1),
        "pawn": pawn_structure_score / total_positions,
        "mobility": mobility_score / total_positions,
        "defense": defense_score / max(middlegame_positions, 1),
        "imbalance": imbalance_score / total_positions,
        "material": calculation_score / total_positions,
    }


def generate_header(check_mode=False):
    base_dir = os.path.dirname(os.path.abspath(__file__))
    pgn_dir = os.path.join(base_dir, "..", "data", "pgn")
    header_path = os.path.join(base_dir, "..", "engine_src", "nen_profiles.h")

    results = {}
    for gm, (fname, style_label, era, match_str) in GM_ARCHETYPES.items():
        fpath = os.path.join(pgn_dir, fname)
        data = analyze_pgn(fpath, match_str)
        if data["games"] < 100:
            print(f"Warning: {gm} dataset has {data['games']} games (expected >= 100)")
        results[gm] = data

    # Compute min-max for each feature
    features = [f[0] for f in FEATURE_TO_TRAIT]
    f_min = {f: min(results[g][f] for g in GM_ARCHETYPES) for f in features}
    f_max = {f: max(results[g][f] for g in GM_ARCHETYPES) for f in features}

    # Derive profiles
    profiles = {}
    for gm in GM_ARCHETYPES:
        prof = {}
        for feat, field_name, _ in FEATURE_TO_TRAIT:
            denom = f_max[feat] - f_min[feat]
            norm = (results[gm][feat] - f_min[feat]) / denom if denom > 0 else 0.5
            # Linear blend: 25..95 mapping, clamped 0..100
            val = int(round(25.0 + 70.0 * norm))
            val = max(0, min(100, val))
            prof[field_name] = val
        profiles[gm] = prof

    # Construct provenance block
    lines = [
        "#pragma once",
        "",
        "// ============================================================================",
        "// Auto-generated by scripts/train_nen_styles.py from Prime-Era GM PGN Datasets",
        "// Maps Chess Playstyle Traits to Engine Evaluation Multipliers",
        "// Reference: docs/nen_system.md and README.md",
        "// ============================================================================",
        "// Provenance & Calibration Metadata:",
        "// Mapping Formula: norm = (feat - min) / (max - min); trait = round(25 + 70 * norm)",
        "// Prime-Era Datasets Analyzed (GM-perspective middlegame/endgame plies > 10):",
    ]
    for gm, (fname, style_label, era, _) in GM_ARCHETYPES.items():
        d = results[gm]
        lines.append(f"//   - {gm:10} ({era}, {style_label:11}): {d['games']:3} games, {d['positions']:5} GM plies")

    lines.append("// Measured Feature Bounds:")
    for feat in features:
        lines.append(f"//   - {feat:12}: min={f_min[feat]:.4f}, max={f_max[feat]:.4f}")

    lines.append("// ============================================================================")
    lines.append("")
    lines.append("struct NenProfile {")
    lines.append("    int enhancement = 50;     // Calculation (0-100): Material precision & piece tension")
    lines.append("    int transmutation = 50;   // Aggression  (0-100): King attacks, sacrifices, initiative")
    lines.append("    int conjuration = 50;     // Structure   (0-100): Pawn structure integrity, chains, fortresses")
    lines.append("    int specialization = 50;  // Creativity  (0-100): Asymmetric imbalances, unorthodox play")
    lines.append("    int manipulation = 50;    // Defense     (0-100): King shelter, prophylaxis, solidity")
    lines.append("    int emission = 50;        // Activity    (0-100): Piece mobility, open files, board control")
    lines.append("};")
    lines.append("")
    lines.append("using StyleProfile = NenProfile;")
    lines.append("")
    lines.append("// Calibrated Prime-Era Grandmaster Profiles (Derived from Data)")
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
        print("OK: Committed nen_profiles.h matches prime-era GM data pipeline.")
        sys.exit(0)
    else:
        with open(header_path, "w", encoding="utf-8") as f:
            f.write(header_text)
        print(f"Successfully generated {header_path}")
        print("\nDerived Prime-Era Grandmaster Profiles (Calc / Aggr / Struct / Creat / Def / Act):")
        for gm in ["Tal", "Petrosian", "Karpov", "Morphy", "Kasparov", "Carlsen"]:
            p = profiles[gm]
            print(
                f"  {gm:10s}: Calc={p['enhancement']:2d}  Aggr={p['transmutation']:2d}  "
                f"Struct={p['conjuration']:2d}  Creat={p['specialization']:2d}  "
                f"Def={p['manipulation']:2d}  Act={p['emission']:2d}"
            )


def main():
    parser = argparse.ArgumentParser(description="Train chess playstyle profiles from prime-era GM PGNs")
    parser.add_argument("--check", action="store_true", help="Check committed header without writing")
    args = parser.parse_args()
    generate_header(check_mode=args.check)


if __name__ == "__main__":
    main()
