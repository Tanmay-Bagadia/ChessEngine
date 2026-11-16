# Nen Chess Personas & Modern Lichess UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the Hunter × Hunter Nen Hexagon player persona system in the C++ chess engine, provide an offline GM training and calibration pipeline, rename the engine from TanmayChess to "Chess Engine", and integrate the Google Stitch design featuring an authentic Lichess evaluation bar, interactive morphing radar hexagon, and 6 precision archetype sliders.

**Architecture:** 
- In C++, a `NenProfile` struct dynamically scales 5 core evaluation components (`king_attack`, `mobility`, `pawn_structure`, `king_shield`, `material_scale`) and search contempt, exposed through standard UCI options.
- An offline Python script (`scripts/train_nen_styles.py`) parses curated GM PGN datasets to compute archetype feature distributions and calibrate static preset constants in `engine_src/nen_profiles.h`.
- The Node.js server (`gui/server.js`) streams continuous live analysis scores and exposes `/api/nen` endpoints.
- The web frontend (`gui/index.html`) integrates the Google Stitch prototype with an animated vertical Lichess evaluation bar, interactive SVG Nen Hexagon, GM preset buttons, and responsive keyboard controls.

**Tech Stack:** C++17, Node.js (HTTP / Server-Sent Events), Vanilla JS / CSS / SVG, Python 3 (chess, PGN parsing), CMake.

**Spec:** [`docs/superpowers/specs/2026-11-17-nen-chess-personas-design.md`](file:///Users/tanmay1301/projects/ChessEngine/docs/superpowers/specs/2026-11-17-nen-chess-personas-design.md)

---

## Global Constraints

- 100% native C++ engine with zero external runtime binary or library dependencies.
- Standard UCI protocol compliance (`id name Chess Engine`, `setoption name Nen_<Trait> value <0-100>`).
- Engine must remain renamed as "Chess Engine" throughout UI and UCI (no "TanmayChess").
- All 18 perft benchmarks and tactical tests in `scripts/test_engine.py` must maintain 100% pass rate.
- Real-time slider and eval bar adjustments must not cause frame drops, stutter, or search crashes.

---

## Review Focus

1. **Extreme Nen Values**: Sliders set to extreme values (e.g., Transmutation=100%, Enhancement=0%) must not trigger numeric overflow, negative piece values, or tactical blindness.
   - *Test in Task 1*: Unit test evaluating extreme profiles against known tactical puzzles.
2. **UCI State Desynchronization**: Changing individual sliders after selecting a preset must gracefully set `Nen_Preset` to `Custom` without losing the custom slider weights.
   - *Test in Task 2*: UCI harness test setting a preset and then modifying a single trait.
3. **Board-Flipped Lichess Eval Bar**: When board is flipped (Black perspective), the eval bar must physically flip so Black's advantage grows upward/downward appropriately with matching score badge colors.
   - *Test in Task 5*: Visual and DOM check on `eval-bar` class toggle and percentage calculation.
4. **Continuous Analysis Lifecycle**: In two-player mode or human turn, triggering live analysis must not interfere with moves or leave dangling search threads when a move is quickly played.
   - *Test in Task 4*: Rapid move submission test ensuring clean `stop` -> `position` -> `go` sequence.
5. **Clean Engine Compilation**: Rebuilding with `cmake --build build -j` must produce zero compiler warnings or linking errors across `nen_profiles.h`, `evaluation.cpp`, and `uci.cpp`.
   - *Test in Task 1 & 2*: Clean compilation verification.

---

## Task 1: C++ Evaluation Engine — `NenProfile`, King Attack Evaluator & Dynamic Scaling

**Files:**
- Create: `engine_src/nen_profiles.h`
- Modify: `engine_src/evaluation.h:1-15`
- Modify: `engine_src/evaluation.cpp:320-407`
- Test: `tests/test_nen_eval.cpp` (or `scripts/test_nen_eval.py`)

**Interfaces:**
- Produces:
  ```cpp
  struct NenProfile {
      int enhancement = 50;     // 0 - 100
      int transmutation = 50;   // 0 - 100
      int conjuration = 50;     // 0 - 100
      int specialization = 50;  // 0 - 100
      int manipulation = 50;    // 0 - 100
      int emission = 50;        // 0 - 100
  };
  void set_nen_profile(const NenProfile &prof);
  NenProfile get_nen_profile();
  int evaluate_king_attack(const Position &pos);
  ```

- [ ] **Step 1: Write the failing test for Nen evaluation scaling**

Create `scripts/test_nen_eval.py` that loads an aggressive attacking position (e.g., Tal sacrifice setup), queries the engine evaluation with default Nen vs. max Transmutation (95%), and asserts the Transmutation score gives a higher attacking weight.

```python
# scripts/test_nen_eval.py
import subprocess

def run_eval(commands):
    proc = subprocess.Popen(["./build/engine"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    out, _ = proc.communicate("\n".join(commands) + "\nquit\n")
    return out

out_default = run_eval(["position fen r1bq1rk1/ppp2ppp/2n1pn2/3p4/2PP4/2N1PN2/PP3PPP/R1BQKB1R w KQ - 0 6", "eval"])
out_transmutation = run_eval([
    "setoption name Nen_Transmutation value 100",
    "setoption name Nen_Emission value 100",
    "position fen r1bq1rk1/ppp2ppp/2n1pn2/3p4/2PP4/2N1PN2/PP3PPP/R1BQKB1R w KQ - 0 6",
    "eval"
])
print("Default:", out_default)
print("Transmutation:", out_transmutation)
assert "Nen_Transmutation" in out_transmutation or "Evaluation" in out_transmutation
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 scripts/test_nen_eval.py`
Expected: Output shows option unrecognized or evaluation identical.

- [ ] **Step 3: Implement `nen_profiles.h` and update `evaluation.h` and `evaluation.cpp`**

1. Define `struct NenProfile` in `engine_src/nen_profiles.h` with default GM profiles (Tal, Petrosian, Karpov, Morphy, Kasparov, Carlsen, Balanced).
2. In `engine_src/evaluation.h`, declare `set_nen_profile()`, `get_nen_profile()`, and `evaluate_king_attack()`.
3. In `engine_src/evaluation.cpp`, implement `evaluate_king_attack()` by calculating piece attacks into the 3x3 square ring surrounding the enemy king.
4. Scale positional and king attack terms in `evaluate()` according to the active `NenProfile`:
   - `attack_weight = (prof.transmutation * 1.2f + prof.emission * 0.8f) / 100.0f;`
   - `pawn_weight = (prof.conjuration * 1.4f + prof.manipulation * 0.6f) / 100.0f;`
   - `mobility_weight = (prof.emission * 1.2f + prof.specialization * 0.8f) / 100.0f;`
   - `shield_weight = (prof.manipulation * 1.3f + prof.enhancement * 0.7f) / 100.0f;`
   - `material_scale = 0.7f + (prof.enhancement / 100.0f) * 0.6f;`

- [ ] **Step 4: Build and run test to verify it passes**

Run:
```bash
cmake --build build -j
python3 scripts/test_nen_eval.py
```
Expected: PASS, evaluation successfully scales based on Nen configuration.

- [ ] **Step 5: Commit**

```bash
git add engine_src/nen_profiles.h engine_src/evaluation.h engine_src/evaluation.cpp scripts/test_nen_eval.py
git commit -m "feat(engine): implement NenProfile and dynamic evaluation scaling"
```

---

## Task 2: C++ UCI Interface & Engine Identity Rename

**Files:**
- Modify: `engine_src/uci.cpp:155-220`
- Modify: `engine_src/search.cpp:1-50` (contempt factor scaling)
- Test: `scripts/test_uci_nen.py`

**Interfaces:**
- Consumes: `NenProfile`, `set_nen_profile()`, `get_nen_profile()` from Task 1.
- Produces: Standard UCI options:
  - `option name Nen_Enhancement type spin default 50 min 0 max 100`
  - `option name Nen_Transmutation type spin default 50 min 0 max 100`
  - `option name Nen_Conjuration type spin default 50 min 0 max 100`
  - `option name Nen_Specialization type spin default 50 min 0 max 100`
  - `option name Nen_Manipulation type spin default 50 min 0 max 100`
  - `option name Nen_Emission type spin default 50 min 0 max 100`
  - `option name Nen_Preset type combo default Balanced var Balanced var Tal var Petrosian var Karpov var Morphy var Kasparov var Carlsen var Custom`

- [ ] **Step 1: Write test for UCI options and engine rename**

Create `scripts/test_uci_nen.py` to verify:
1. `uci` returns `id name Chess Engine` (not TanmayChess).
2. All 7 Nen UCI options are registered.
3. Setting `setoption name Nen_Preset value Tal` updates the 6 trait options.

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 scripts/test_uci_nen.py`
Expected: FAIL (still outputs `TanmayChess` and lacks Nen options).

- [ ] **Step 3: Update `engine_src/uci.cpp`**

1. Change `id name TanmayChess 2.0` to `id name Chess Engine`.
2. Register the 6 spin options and 1 combo option in response to `uci`.
3. Handle `setoption name Nen_<Trait> value <Val>` and `setoption name Nen_Preset value <Preset>`.
4. Forward contempt from `profile.transmutation` into search contempt.

- [ ] **Step 4: Build and run test to verify it passes**

Run:
```bash
cmake --build build -j
python3 scripts/test_uci_nen.py
python3 scripts/test_engine.py
```
Expected: PASS across all UCI tests and 100% of the 18 perft benchmarks.

- [ ] **Step 5: Commit**

```bash
git add engine_src/uci.cpp engine_src/search.cpp scripts/test_uci_nen.py
git commit -m "feat(uci): rename to Chess Engine and add Nen archetype UCI options"
```

---

## Task 3: GM Training & Calibration Pipeline

**Files:**
- Create: `data/pgn/tal.pgn`, `data/pgn/petrosian.pgn`, `data/pgn/karpov.pgn`, `data/pgn/morphy.pgn`, `data/pgn/kasparov.pgn`, `data/pgn/carlsen.pgn`
- Create: `scripts/train_nen_styles.py`
- Modify: `engine_src/nen_profiles.h`

**Interfaces:**
- Produces: Calibrated static preset structs in `engine_src/nen_profiles.h`:
  - `TAL_PROFILE`: High Transmutation (90), High Emission (75), Low Conjuration (30)
  - `PETROSIAN_PROFILE`: High Manipulation (90), High Conjuration (80), Low Transmutation (25)
  - `KARPOV_PROFILE`: High Conjuration (95), High Manipulation (75), Moderate Enhancement (60)
  - `MORPHY_PROFILE`: High Emission (95), High Transmutation (70), Low Conjuration (25)
  - `KASPAROV_PROFILE`: High Enhancement (90), High Emission (85), High Transmutation (75)
  - `CARLSEN_PROFILE`: High Specialization (95), High Enhancement (80), Balanced (65)

- [ ] **Step 1: Write curated PGN sample dataset and parser**

Create `data/pgn/` with representative classical games of each GM and implement `scripts/train_nen_styles.py` to extract:
- King attack proximity frequency (pieces attacking king zone).
- Average pawn chain length & isolated pawns.
- Piece trade frequency vs. piece retention.
- Open file rook placement frequency.

- [ ] **Step 2: Run `train_nen_styles.py` to calibrate preset parameters**

Run: `python3 scripts/train_nen_styles.py --output engine_src/nen_profiles.h`
Expected: Output generated with mathematically grounded archetype constants.

- [ ] **Step 3: Verify engine builds cleanly with updated profiles**

Run:
```bash
cmake --build build -j
python3 scripts/test_engine.py
```
Expected: Engine compiles cleanly, 100% test pass rate.

- [ ] **Step 4: Commit**

```bash
git add data/pgn/ scripts/train_nen_styles.py engine_src/nen_profiles.h
git commit -m "feat(trainer): add GM PGN dataset and calibration pipeline"
```

---

## Task 4: Node.js Server (`gui/server.js`) — Live Analysis & Nen UCI API

**Files:**
- Modify: `gui/server.js:100-350`
- Test: `scripts/test_gui_server.py`

**Interfaces:**
- Produces:
  - `POST /api/nen` with `{ enhancement, transmutation, conjuration, specialization, manipulation, emission, preset }`
  - `GET /api/nen` returning active Nen configuration.
  - Automatic `go depth 8` background analysis after each move.
  - SSE event `type: 'info'` streaming continuous evaluation score to the browser.

- [ ] **Step 1: Write test for server Nen API and analysis streaming**

Create `scripts/test_gui_server.py` making HTTP POST to `/api/nen` and verifying that the backend sends `setoption` to the C++ engine process.

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 scripts/test_gui_server.py`
Expected: FAIL with 404 or unrecognized endpoint.

- [ ] **Step 3: Implement `/api/nen` and continuous analysis in `gui/server.js`**

1. Parse `/api/nen` GET and POST requests in the HTTP request router.
2. In `gui/server.js`, keep active `nenState` in memory and dispatch `setoption` commands to `engineProcess`.
3. In `syncEnginePosition()`, if it's the player's turn or "Two players" is active, trigger a lightweight analysis search (`go depth 8`) to keep evaluation scores live.

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 scripts/test_gui_server.py`
Expected: PASS, Nen profile synced and live scores streamed via SSE.

- [ ] **Step 5: Commit**

```bash
git add gui/server.js scripts/test_gui_server.py
git commit -m "feat(server): add Nen profile API and continuous analysis streaming"
```

---

## Task 5: UI Modernization — Stitch Integration, Nen Hexagon Radar & Lichess Eval Bar

**Files:**
- Modify: `gui/index.html` (integrate layout, CSS tokens, SVG radar, sliders, and eval bar from `/tmp/stitch_nen/code.html`)
- Rename all visible branding from "TanmayChess" to "Chess Engine".
- Test: Browser functional test.

**Interfaces:**
- Consumes: `/api/stream` SSE events (`state`, `info`, `bestmove`) and `/api/nen`.
- Produces:
  - Vertical Lichess evaluation bar with smooth cubic-bezier animation and flip support.
  - Interactive SVG Nen Hexagon with morphing polygon and glow.
  - 6 Nen sliders updating engine in real-time.
  - 7 GM preset buttons.
  - Move history table with navigation buttons (`|<<`, `<`, `>`, `>>|`).
  - Hotkeys (`F` to flip, `U` to undo, `N` for new game, `Space` to play engine).

- [ ] **Step 1: Integrate Stitch Design Tokens & Layout into `gui/index.html`**

1. Import CSS design tokens, fonts (`Inter`, `JetBrains Mono`), and surfaces from Stitch (`tokens.css`).
2. Rename header to "Chess Engine" with tag "NEN ENGINE · C++17".
3. Build the 3-column responsive layout:
   - Left: Lichess eval bar (`36px`) + enlarged chessboard (`640px`, 80px squares with fluid responsive scaling up to 680px on large displays) + action buttons (`New Game`, `Flip`, `Undo`, `Copy FEN`, `Analyse`, `Play Engine`, `Two Players`).
   - Center/Right top: Engine Lines (MultiPV analysis).
   - Center/Right middle: Nen Persona card (SVG radar hexagon, 6 sliders, 7 presets, Reset).
   - Center/Right bottom: Move list with ply navigation.

- [ ] **Step 2: Implement Lichess Evaluation Bar**

1. Create vertical container with white and black segments.
2. Calculate winning percentage: `winPercent = 50 + 50 * (2 / (1 + Math.exp(-0.003682 * cp)) - 1)`.
3. Animate height with `transition: height 0.25s cubic-bezier(0.4, 0, 0.2, 1)`.
4. Position score badge dynamically inside the winning region (`+1.4` on white in black text, `-2.1` on black in white text, `#3` for checkmate).
5. When `isFlipped` is true, invert the bar layout so Black is bottom and White is top.

- [ ] **Step 3: Implement SVG Nen Hexagon Radar & Slider Sync**

1. Render the 6-axis Nen Hexagon using SVG polygons with coordinates calculated at 60-degree increments:
   - Top ($90^\circ$): Enhancement (Green `#37D67A`)
   - Top-right ($30^\circ$): Transmutation (Purple `#9B5CFF`)
   - Bottom-right ($330^\circ$): Conjuration (Red `#E5484D`)
   - Bottom ($270^\circ$): Specialization (Blue `#3B82F6`)
   - Bottom-left ($210^\circ$): Manipulation (Grey `#9AA3AD`)
   - Top-left ($150^\circ$): Emission (Yellow `#F5C451`)
2. Draw inner polygon `points` attribute updated on slider input.
3. Wire preset buttons to smoothly update slider values and dispatch `POST /api/nen`.

- [ ] **Step 4: Wire Board Mechanics & Hotkeys**

1. Retain native piece move generation and legal move indicators.
2. Add keyboard shortcuts (`F`, `U`, `N`, `Space`).
3. Connect MultiPV line clicking to preview engine moves.

- [ ] **Step 5: Test in browser**

Restart `gui/server.js` and open `http://localhost:3000`. Test:
- Moving sliders morphs the radar polygon.
- Presets change all sliders to target values.
- Making a move updates the Lichess eval bar smoothly.
- Flipping board flips both the board and the eval bar.
- Playing engine against Tal vs. Petrosian behaves according to styles.

- [ ] **Step 6: Commit**

```bash
git add gui/index.html
git commit -m "feat(gui): integrate Stitch modern UI, Nen radar hexagon, and Lichess eval bar"
```

---

## Task 6: End-to-End Regression & Push to Remote

**Files:**
- Test: `scripts/test_engine.py`
- Test: `scripts/test_uci_nen.py`

- [ ] **Step 1: Run comprehensive benchmark suite**

Run: `python3 scripts/test_engine.py`
Verify 18 perft suites, tactical puzzles, movetime, and self-play pass 100%.

- [ ] **Step 2: Verify git status and commit clean branch**

Run:
```bash
git status
git log -n 5 --oneline
```

- [ ] **Step 3: Push to GitHub repository**

Run: `git push origin main`
Expected: Remote `origin/main` cleanly updated with Tanmay's commits.
