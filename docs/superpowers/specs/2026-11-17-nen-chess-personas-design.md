# Design Document: Nen Chess Persona System & Modern Lichess UI

- **Date**: 2026-11-17
- **Topic**: Hunter × Hunter Nen Hexagon Engine Personas, GM Training Pipeline, and Lichess-Grade UI
- **Status**: Approved & Ready for Planning

---

## 1. Intent & The "Why"

The user wants to make TanmayChess stronger while giving it distinct, customizable playing styles inspired by the **Hunter × Hunter Nen Hexagon** (Enhancement, Transmutation, Conjuration, Specialization, Manipulation, Emission). Instead of a generic single-style engine, the engine learns from games of legendary Grandmasters representing each archetype and allows the player to configure individual affinity traits via continuous sliders and an interactive radar hexagon widget. In addition, the GUI will receive a visual overhaul, including an authentic, responsive Lichess-style evaluation bar.

---

## 2. Goals, Non-Goals & Anti-Goals

### Goals

- **Nen Archetype System**: Map the 6 Nen affinities into concrete chess evaluation and search parameters in C++.
- **GM Training Pipeline**: Curate PGN datasets of legendary Grandmasters for each archetype and train/tune style weight vectors.
- **Continuous Customization**: Provide 6 sliders (0–100%) and Grandmaster presets that immediately reconfigure the engine's evaluation terms via UCI `setoption` commands without restarting the engine.
- **Authentic Lichess Evaluation Bar**: Vertical bar adjacent to the board with smooth cubic-bezier transitions, automatic board-flip orientation support, continuous live analysis updates after every move, and a score indicator dynamically positioned inside the winning player's color block.
- **Polished UI & Nen Radar Widget**: Render an interactive Nen Hexagon radar diagram with colored aura vertices matching the manga color scheme (Green, Purple, Red, Blue, Grey, Yellow), responsive board controls, and modern dark aesthetics.

### Non-Goals

- Replacing the core C++ move generation or perft verification with external libraries.
- Training heavy multi-gigabyte neural networks requiring days of GPU time; the engine must run lean and fast with instant responsiveness.

### Anti-Goals

- Compromising engine tactical stability: no style setting should cause the engine to blunder simple one-move tactics or basic checkmates.
- Sluggish GUI updates: slider movements and eval bar changes must be smooth and lag-free.

---

## 3. Constraints

- The engine must remain 100% native C++ with zero external runtime dependencies.
- All UCI communication must follow standard UCI protocols so external GUIs can also use the Nen options.
- All perft tests (benchmarks 1–18) and existing tactical test suites (`scripts/test_engine.py`) must continue to pass at 100%.

---

## 4. Approaches Considered

### Approach 1: Feature-Weight Tuning from GM Game Collections + Continuous Nen Hexagon Sliders (Selected)

- **Mechanism**: Curate PGN game collections for each archetype, extract evaluation weights, and implement dynamic scaling in `evaluation.cpp` and `search.cpp` controlled by 6 UCI parameters.
- **Pros**: Instant real-time slider updates, robust tactical preservation, zero external runtime dependencies, fast and verifiable training pipeline.
- **Cons**: Requires careful parameter normalization so extreme slider values don't create erratic evaluations.

### Approach 2: Multi-Model NNUE Switching (6 Dedicated Neural Networks)

- **Mechanism**: Train 6 separate PyTorch NNUE networks, one for each archetype.
- **Pros**: Neural representation of subtle piece combinations.
- **Cons**: High training time, requires 100k+ games per model, cannot smoothly blend continuous intermediate slider percentages (e.g. 35% Tal + 65% Petrosian), and high memory footprint.

### Approach 3: Search-Only Contempt Tuning

- **Mechanism**: Only tweak search pruning, aspiration windows, and contempt factors without touching evaluation terms.
- **Pros**: Minimal code changes.
- **Cons**: Styles feel indistinguishable in quiet and balanced positions; fails to capture distinct pawn structure and piece coordination preferences.

---

## 5. Detailed Design & Architecture

### 5.1 The 6 Nen Affinities & Concrete Evaluation Terms

The 6 Nen affinities map to 5 core classical evaluation components and search contempt in C++:

1. **Enhancement (強化 - Green - Kasparov / Fischer)**:
   - Scales `material_scale` and base piece values.
   - Tightens search aspiration windows for pure brute-force accuracy.
2. **Transmutation (変化 - Purple - Tal / Nezhmetdinov)**:
   - Scales `king_attack_bonus` (points for pieces attacking squares around the enemy king).
   - Increases search contempt (willingness to sacrifice material to avoid drawish repetitions).
3. **Conjuration (具現化 - Red - Karpov / Capablanca)**:
   - Scales `pawn_structure_bonus` (penalties for doubled/isolated pawns, bonuses for passed pawns and protected chains).
4. **Specialization (特質 - Blue - Carlsen / Larsen)**:
   - Scales asymmetric piece balance (bishop pair vs. knights, rook coordination).
   - Introduces slight move-order volatility in equal positions for surprise value.
5. **Manipulation (操作 - Grey - Petrosian / Nimzowitsch)**:
   - Scales `king_shield_bonus` (defense of friendly king).
   - Scales opponent mobility restriction penalties (suffocating counterplay).
6. **Emission (放出 - Yellow - Morphy / Marshall)**:
   - Scales `mobility_bonus` and open/semi-open file rook bonuses.
   - Contributes to `king_attack_bonus` via long-range pieces.

### 5.2 C++ Engine Implementation (`engine_src/evaluation.h`, `engine_src/evaluation.cpp`, `engine_src/uci.cpp`)

1. **`NenProfile` Struct & State Authority**:
   ```cpp
   struct NenProfile {
       int enhancement = 50;     // 0 - 100
       int transmutation = 50;   // 0 - 100
       int conjuration = 50;     // 0 - 100
       int specialization = 50;  // 0 - 100
       int manipulation = 50;    // 0 - 100
       int emission = 50;        // 0 - 100
   };
   ```

   - The 6 integer sliders are the **sole authoritative engine state**.
   - Presets (e.g. Tal, Petrosian) are convenience configurations that populate the 6 sliders.
2. **Evaluation Functions**:
   - Introduce `evaluate_king_attack(const Position &pos)` to count attacking piece weights in the 3x3 zone around the enemy king.
   - Scale terms dynamically:
     ```cpp
     float attack_mult = (profile.transmutation * 1.2f + profile.emission * 0.8f) / 100.0f;
     float pawn_mult   = (profile.conjuration * 1.4f + profile.manipulation * 0.6f) / 100.0f;
     float mob_mult    = (profile.emission * 1.2f + profile.specialization * 0.8f) / 100.0f;
     float shield_mult = (profile.manipulation * 1.3f + profile.enhancement * 0.7f) / 100.0f;
     float mat_mult    = 0.7f + (profile.enhancement / 100.0f) * 0.6f;
     ```
3. **UCI Commands**:
   - `option name Nen_Enhancement type spin default 50 min 0 max 100`
   - `option name Nen_Transmutation type spin default 50 min 0 max 100`
   - `option name Nen_Conjuration type spin default 50 min 0 max 100`
   - `option name Nen_Specialization type spin default 50 min 0 max 100`
   - `option name Nen_Manipulation type spin default 50 min 0 max 100`
   - `option name Nen_Emission type spin default 50 min 0 max 100`
   - `option name Nen_Preset type combo default Balanced var Balanced var Tal var Petrosian var Karpov var Morphy var Kasparov var Carlsen var Custom`

### 5.3 Offline GM Training Pipeline (`scripts/train_nen_styles.py`)

- Curate PGN collections of representative GM games into `data/pgn/`:
  - `tal.pgn` (Transmutation)
  - `petrosian.pgn` (Manipulation)
  - `karpov.pgn` (Conjuration)
  - `morphy.pgn` (Emission)
  - `kasparov.pgn` (Enhancement)
  - `carlsen.pgn` (Specialization)
- `train_nen_styles.py` parses these games, extracts statistical distributions of attacks, pawn structures, piece trades, and mobility, and generates a standalone C++ header: `engine_src/nen_profiles.h`.
- The engine compiles this header directly into the executable, preserving zero runtime dependencies.

### 5.4 Lichess-Grade Evaluation Bar (`gui/index.html`, `gui/server.js`)

- **Visuals & Mechanics**:
  - Vertical bar `32px` wide, matching the full height of the chessboard (`560px`).
  - Dark region (`#262421`) for Black and crisp white region (`#ffffff`) for White.
  - Height transition: `transition: height 0.25s cubic-bezier(0.4, 0, 0.2, 1);`.
  - Flip support: When the board is flipped (Black at bottom), the eval bar also flips (Black at bottom, White at top).
- **Dynamic Score Badge**:
  - Placed inside the winning player's territory:
    - If White is winning ($>0$), badge floats near the bottom on the white background in dark text (`+1.5`).
    - If Black is winning ($<0$), badge floats near the top on the dark background in white text (`-2.3`).
    - Mate announcements (`M2`, `-M3`).
- **Continuous Live Analysis**:
  - `gui/server.js` triggers a background analysis search (`go depth 8`) whenever any move is played (including when the user plays a move or in two-player analysis mode), keeping the eval bar continuously updated just like Lichess.

### 5.5 Modern UI & Nen Hexagon Widget

- **Interactive SVG Radar Hexagon**:
  - 6 vertices with authentic Hunter × Hunter labels (強化, 変化, 具現化, 特質, 操作, 放出).
  - Morphing radar polygon with glowing aura gradients reflecting the dominant trait.
  - Interactive slider controls with real-time feedback and GM preset buttons:
    - **Mikhail Tal** (Attack & Sacrifices)
    - **Tigran Petrosian** (Iron Defense & Prophylaxis)
    - **Anatoly Karpov** (Pawn Structure & Technical Conversion)
    - **Paul Morphy** (Open Lines & Rapid Development)
    - **Garry Kasparov** (Aggressive Brute-Force Mastery)
    - **Magnus Carlsen** (Endgame & Asymmetric Imbalances)
    - **Balanced Nen** (Default neutral engine)
- **UI Modernization**:
  - Clean glassmorphism container styling, updated typography, and responsive move history.
  - Automatic synchronization: adjusting any Nen slider while in NNUE mode smoothly switches evaluation to Classical Nen mode.

---

## 6. Verification Plan

1. **Engine Benchmarks**: Run `python3 scripts/test_engine.py` to verify that all 18 perft benchmarks and tactical tests pass with 100% accuracy across default and extreme Nen profiles.
2. **UCI Verification**: Test `setoption name Nen_Transmutation value 90` and verify the engine's evaluation shifts accordingly in king attack positions.
3. **UI Verification**: Open `http://localhost:3000`, test dragging the Nen sliders, selecting presets, verifying the radar polygon morphs, flipping the board to verify the Lichess eval bar flips smoothly, and playing moves to confirm real-time updates.
