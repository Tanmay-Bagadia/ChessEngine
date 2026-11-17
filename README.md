# Chess Engine

A C++17 bitboard chess engine featuring **PeSTO classical evaluation with configurable Grandmaster playstyles**, an **incremental ARM NEON SCReLU NNUE neural network**, full **UCI protocol** support with MultiPV, and a **real-time web analysis GUI**.

---

## Strength & Benchmarks

Evaluated head-to-head against calibrated **Stockfish 19** (`UCI_LimitStrength` / `UCI_Elo`, `0.2s/move`, varied opening book pairs on Apple Silicon):

| Evaluation Mode | Speed (Depth 9) | vs Stockfish 1700 | vs Stockfish 2000 | vs Stockfish 2300 | vs Stockfish 2500 | Estimated Strength |
|---|---|---|---|---|---|---|
| **Classical PeSTO** | ~1.10M NPS | `4.0 – 0.0` | `3.0 – 1.0` | `2.5 – 1.5` | `2.0 – 2.0` | **~2450–2500 Elo** *(UCI)* |
| **NNUE v3 (SCReLU + NEON)** | ~585k NPS | — | `2.5 – 1.5` | `1.5 – 2.5` | `2.0 – 2.0` | **~2250–2450 Elo** *(UCI)* |

- **Perft Correctness:** **18 / 18** standard benchmark positions pass (Startpos Depth 1–5, Kiwipete Depth 1–4, Positions 3–5 Depth 1–3).

---

## Architecture & Features

### Core Engine (`engine_src/`)
- **Bitboards & Magic Bitboards:** 64-bit piece bitboards with precomputed magic lookup tables for sliding piece attacks (bishops, rooks, queens).
- **Incremental Make/Unmake:** Zero-copy `make_move` / `unmake_move` with `UndoInfo`, incrementally updating Zobrist hashes, tapered PeSTO scores, game phase, and NNUE accumulators.
- **Search:** Negamax with Alpha-Beta pruning, Principal Variation Search (PVS), Iterative Deepening, Aspiration Windows, Age-Based Transposition Table, Quiescence Search with Static Exchange Evaluation (SEE), Null Move Pruning, Late Move Reductions (LMR), Futility Pruning, Late Move Pruning (LMP), Razoring, Check Extensions, Mate Distance Pruning, and **root MultiPV (`1..5`)**.
- **Move Ordering:** Hash move → MVV-LVA captures → Promotions → Killer moves → Counter-move heuristic → History heuristic.
- **Thread-Safe UCI:** Atomic search IDs with non-blocking bounded `stop_search` prevent stale `bestmove` leaks and UI freezes.

### Dual Evaluation
1. **Classical PeSTO + Playstyle Modulation:** Tapered Middlegame/Endgame PeSTO piece-square tables combined with pawn structure (doubled, isolated, passed), piece mobility, bishop pair, open/semi-open file rooks, and king safety—modulated dynamically across 6 playstyle settings.
2. **NNUE v3 (`(768 → 256)×2 → 1` SCReLU):** Shallow Squared Clipped ReLU neural network quantized to `int16` (`QA=181, QB=64`), updated incrementally in `make_move`/`unmake_move` with **ARM NEON SIMD** vectorization (portable scalar fallback on non-ARM targets). Trained via hybrid Stockfish 19 static NNUE + PeSTO distillation on **117,780** diverse prime-era GM and endgame positions (**221,666** mirrored training vectors).

### Playstyle Settings & Prime-Era GM Profiles
Configurable in real time via sliders, presets, or UCI options (`Style_Calculation`, `Style_Aggression`, `Style_Structure`, `Style_Creativity`, `Style_Defense`, `Style_Activity`, `Style_Preset`). Changing any playstyle slider automatically switches to Classical evaluation.

Presets are empirically calibrated from **900 prime-era classical games** (150 per player, 30,778 GM-perspective plies):

| Grandmaster | Prime Era | Calc | Aggr | Struct | Creat | Def | Act |
|---|---|---|---|---|---|---|---|
| **Mikhail Tal** | 1957–1961 | 61% | 56% | 43% | 79% | 62% | 80% |
| **Tigran Petrosian** | 1962–1969 | 31% | 25% | 95% | 25% | 95% | 25% |
| **Anatoly Karpov** | 1974–1985 | 25% | 31% | 85% | 63% | 93% | 64% |
| **Paul Morphy** | 1857–1859 | 95% | 95% | 25% | 95% | 25% | 95% |
| **Garry Kasparov** | 1985–2000 | 49% | 48% | 65% | 56% | 92% | 69% |
| **Magnus Carlsen** | 2013–2021 | 54% | 43% | 27% | 43% | 70% | 78% |
| **Balanced** | — | 50% | 50% | 50% | 50% | 50% | 50% |

### Web GUI (`gui/`)
- Dark Lichess-inspired analysis workspace served via Node.js + Server-Sent Events (SSE) with 100% native C++ move validation.
- Live evaluation bar, MultiPV candidate lines, interactive 6-axis playstyle radar & sliders, PESTO/NNUE mode switcher, HTML5 drag-and-drop & click-to-move, promotion modal (`Escape`/backdrop cancel), custom FEN loader, and keyboard shortcuts (`F`, `U`, `N`, `Space`).

---

## Quick Start

### Prerequisites
- C++17 compiler (Clang / GCC / MSVC) & CMake 3.16+
- Node.js 18+ (for the Web GUI)
- Python 3.9+ (for testing, training pipelines, and 1v1 engine matches)

### 1. Build the C++ Engine
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```
The compiled binary is written to `build/engine`.

### 2. Run the Web GUI
```bash
node gui/server.js
```
Open **http://localhost:3000** in your browser.

---

## UCI Protocol & Custom Commands

Standard UCI commands (`uci`, `isready`, `ucinewgame`, `position`, `go`, `stop`, `quit`, `setoption`) plus diagnostic helpers:

```text
setoption name Hash value 64
setoption name MultiPV value 3
setoption name UseNNUE value true
setoption name Style_Preset value Tal
setoption name Style_Aggression value 85
setoption name Style_Defense value 70

d / display              # Print ASCII board & FEN
eval                     # Static evaluation from side-to-move perspective
evalfen <fen>            # Fast single-line static evaluation of a FEN
perftsuite [max_depth]   # Run built-in perft verification suite
go perft <depth>         # Perft divide
legalmoves / state       # Legal move list / structured GUI state
```

---

## Testing, Training & Engine Matches

### Setup Python Environment
```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

### Run Full Verification Suite (7 Suites)
```bash
.venv/bin/python scripts/run_all_tests.py
```
Covers: playstyle provenance CI gate (`--check`), 18/18 perft benchmarks & tactics, NNUE accuracy & color symmetry, playstyle eval deltas, UCI option advertising, 5× game-flow anti-duplication regression, and GUI REST/SSE endpoints.

### Re-Train Playstyle Profiles or NNUE Weights
```bash
# Re-derive engine_src/nen_profiles.h from data/pgn/*.pgn
.venv/bin/python scripts/train_nen_styles.py
.venv/bin/python scripts/train_nen_styles.py --check

# Re-train NNUE v3 (installs torch + uses Stockfish if available, else PeSTO)
.venv/bin/pip install torch numpy
.venv/bin/python nnue_trainer/train.py
```

### Run 1v1 Matches Against Stockfish or Other UCI Engines
```bash
# Benchmark NNUE v3 against Stockfish at 2000 Elo (varied opening book pairs)
.venv/bin/python scripts/match.py \
  --e1 ./build/engine --e1-nnue \
  --e2 /opt/homebrew/bin/stockfish --e2-elo 2000 \
  --movetime 0.2 --games 6 --openings --pgn match.pgn

# Playstyle showdown (Tal vs Petrosian)
.venv/bin/python scripts/match.py \
  --e1-style Tal --e2-style Petrosian \
  --movetime 0.2 --games 6 --openings
```

---

## Project Structure

```text
ChessEngine/
├── CMakeLists.txt              # C++17 CMake build configuration
├── README.md                   # Project documentation
├── requirements.txt            # Python dependencies
├── nnue.bin                    # Quantized NNUE v3 (SCReLU) weights
├── engine_src/                 # Native C++17 chess engine
│   ├── main.cpp                # Entry point & subsystem initialization
│   ├── bitboard.h / .cpp       # 64-bit bitboards & bit manipulation
│   ├── position.h / .cpp       # Board state, FEN, incremental make/unmake
│   ├── movegen.h / .cpp        # Magic bitboards, move generation, SEE
│   ├── evaluation.h / .cpp     # Tapered PeSTO + dynamic playstyle evaluation
│   ├── nen_profiles.h          # Data-derived prime-era GM profiles
│   ├── nnue.h / .cpp           # Incremental ARM NEON SCReLU NNUE inference
│   ├── search.h / .cpp         # Alpha-Beta/PVS, TT, MultiPV, pruning
│   ├── zobrist.h / .cpp        # Incremental Zobrist position hashing
│   ├── perft.h / .cpp          # Perft & divide verification harness
│   └── uci.h / .cpp            # Thread-safe UCI protocol loop
├── gui/                        # Local Web Analysis GUI
│   ├── index.html              # Interactive board, eval bar, radar & controls
│   ├── server.js               # Node.js UCI-to-REST/SSE bridge
│   ├── package.json            # Node metadata
│   └── pieces/                 # Local SVG chess piece assets
├── nnue_trainer/               # PyTorch NNUE v3 distillation & export pipeline
│   ├── model.py                # SCReLU (768->256)x2->1 architecture & NNv3 exporter
│   ├── dataset.py              # Feature extraction (Simple768)
│   └── train.py                # Hybrid Stockfish 19 + PeSTO training script
├── data/pgn/                   # Prime-era classical GM PGN datasets (900 games)
└── scripts/                    # Test suites, training CLI, and 1v1 match runner
    ├── run_all_tests.py        # Master CI test runner
    ├── engine_path.py          # Central binary path resolver
    ├── match.py                # 1v1 UCI match & Elo benchmark runner
    ├── train_nen_styles.py     # Playstyle feature extractor & header generator
    ├── test_engine.py          # 18/18 perft, tactics, movetime & self-play
    ├── test_nnue_accuracy.py   # NNUE scaling, symmetry & path loading tests
    ├── test_nen_eval.py        # Playstyle behavioral eval delta assertions
    ├── test_uci_nen.py         # UCI protocol & preset option tests
    ├── test_game_flow.py       # 5x anti-duplication game flow regression
    └── test_gui_server.py      # REST & SSE endpoint verification
```

---

## License

MIT License — open source and free to use, modify, and distribute.
