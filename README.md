# ♟️ Chess Engine - Modern Chess Engine with Dynamic Playstyles

A high-performance chess engine written in C++17 with a web-based GUI. Features bitboard representation, magic bitboards for sliding piece attacks, sophisticated search algorithms, and both classical (PeSTO) and NNUE evaluation.

## 🚀 Features

### Engine
- **Bitboard Representation** — Efficient 64-bit bitboards for all piece types
- **Magic Bitboards** — Pre-computed attack tables for bishops and rooks using magic numbers
- **Zobrist Hashing** — Incrementally updated position hashing
- **Make/Unmake Move** — Efficient position updates with undo support (no full copies in search)
- **Incremental Evaluation** — Material + PST scores updated incrementally during make/unmake

### Search
- **Negamax with Alpha-Beta Pruning** — Core search algorithm
- **Principal Variation Search (PVS)** — Narrow windows for non-PV moves
- **Iterative Deepening** — Progressive depth increase with time management
- **Aspiration Windows** — Narrow search windows around expected score
- **Transposition Table** — Hash table with age-based replacement scheme
- **Quiescence Search** — Captures-only search at leaf nodes with stand-pat evaluation
- **Null Move Pruning** — Adaptive R = 3 + depth/6
- **Late Move Reduction (LMR)** — Deeper reductions for late quiet moves
- **Mate Distance Pruning** — Tighten alpha/beta near mate scores
- **Check Extensions** — Extend search when in check
- **Static Exchange Evaluation (SEE)** — Fast piece exchange evaluation

### Move Ordering
- **Hash Move** — TT best move searched first
- **MVV-LVA** — Most Valuable Victim / Least Valuable Attacker for captures
- **Killer Moves** — Two killer moves per ply
- **Counter Move Heuristic** — Good responses to opponent's previous move
- **History Heuristic** — Depth-squared bonuses with clamping

### Evaluation & Playstyles
- **Nen Playstyle System** — Dynamic modulation of evaluation weights across 6 Hunter × Hunter affinities (Enhancement/Calculation, Transmutation/Aggression, Conjuration/Structure, Specialization/Creativity, Manipulation/Prophylaxis, Emission/Activity)
- **Calibrated Grandmaster Archetypes** — 6 empirically trained profiles (Tal, Petrosian, Karpov, Morphy, Kasparov, Carlsen) + Balanced Nen derived from tournament PGN analysis
- **Tapered Evaluation** — Smooth interpolation between midgame and endgame
- **PeSTO Piece-Square Tables** — Well-tuned positional values
- **Pawn Structure** — Doubled, isolated, and passed pawn evaluation
- **Mobility** — Piece mobility bonuses for knights, bishops, rooks, queens
- **Bishop Pair Bonus** — Reward for having both bishops
- **Rook on Open/Semi-Open Files** — Bonus for rooks on files without friendly/enemy pawns
- **King Safety** — Pawn shield evaluation around the king
- **NNUE Support** — Optional neural network evaluation (768→256→32→32→1 architecture) with automatic fallback to classical Nen eval when style options are modified

### UCI Protocol
- Full UCI protocol support with thread-safe search interruption (atomic search IDs)
- `position`, `go`, `stop`, `quit`, `isready`, `ucinewgame`
- MultiPV support (`setoption name MultiPV value 3`) with distinct PV outputs
- Style & Nen UCI options: `Style_Calculation`, `Style_Aggression`, `Style_Structure`, `Style_Creativity`, `Style_Prophylaxis`, `Style_Activity`, `Style_Preset` (and `Nen_*` aliases)
- Custom commands: `d` (display), `eval`, `bench`, `fen`, `legalmoves`, `state`, `perftsuite`

### Web GUI
- Modern dark-themed interface (Lichess-inspired layout; see reference in `docs/stitch/screen.png`)
- Real-time engine analysis via Server-Sent Events (SSE)
- Lichess-grade evaluation bar with smooth winning chance sigmoid
- Real MultiPV display (rows 1-3)
- Interactive Nen Persona Hexagon Radar with dynamic stroke, gradient stops, and aura glow
- Full HTML5 piece drag-and-drop movement and click-to-move
- Pawn promotion modal with backdrop click and Escape key cancellation
- Segmented control for PESTO (Classical) vs NNUE evaluation
- Custom FEN string loader, flip board, move history, play against engine toggle

## 🏗️ Building

### Prerequisites
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.16+
- Node.js (for the web GUI)

### Build the Engine

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j$(nproc)
```

The engine binary will be at `build/engine`.

### Run the GUI

```bash
cd gui
npm install
node server.js
```

Then open `http://localhost:3000` in your browser.

## 📊 Usage

### UCI Commands

```
uci                           # Handshake
isready                       # Wait for ready
position startpos             # Set starting position
position startpos moves e2e4 e7e5  # Play moves
position fen <fen_string>     # Load FEN
go depth 10                   # Search to depth 10
go movetime 1000              # Search for 1 second
go wtime 60000 btime 60000 winc 1000 binc 1000  # Time control
go infinite                   # Search until stop
stop                          # Stop searching
quit                          # Exit
```

### Custom Commands

```
d / display                   # Print board
eval                          # Show static evaluation
bench [depth]                 # Run benchmark (default depth 8)
fen                           # Export current FEN
legalmoves                    # List legal moves
state                         # Full game state (for GUI)
perftsuite [max_depth]        # Run perft test suite
go perft <depth>              # Perft divide
```

## 📁 Project Structure

```
ChessEngine/
├── CMakeLists.txt            # Build configuration
├── README.md                 # This file
├── engine_src/
│   ├── main.cpp              # Entry point
│   ├── bitboard.h/cpp        # Bitboard utilities
│   ├── position.h/cpp        # Position representation, make/unmake
│   ├── movegen.h/cpp         # Move generation, SEE
│   ├── search.h/cpp          # Search algorithm
│   ├── evaluation.h/cpp      # Position evaluation
│   ├── perft.h/cpp           # Perft testing
│   ├── uci.h/cpp             # UCI protocol
│   ├── zobrist.h/cpp         # Zobrist hashing
│   └── nnue.h/cpp            # NNUE evaluation
├── gui/
│   ├── index.html            # Web GUI
│   ├── server.js             # Node.js server (engine ↔ GUI bridge)
│   ├── package.json          # Node.js dependencies
│   └── pieces/               # SVG chess pieces
├── nnue_trainer/             # NNUE training scripts (Python)
├── scripts/
│   └── test_engine.py        # Engine test suite
└── nnue.bin                  # Pre-trained NNUE weights
```

## 🔮 Nen Playstyle System

The engine features a dynamic persona and playstyle system inspired by the six Nen affinities from *Hunter × Hunter*. Instead of a rigid evaluation function, the engine dynamically modulates its evaluation weights across six fundamental dimensions:

| Affinity (Nen) | Kanji | Chess Trait | Primary Evaluation Focus | Multiplier Scaling |
|---|---|---|---|---|
| **Enhancement** | 強化 | **Calculation** | Material valuation, piece values, tactical sharpness | Base material scores |
| **Transmutation** | 変化 | **Aggression** | King attack pressure, kingside storms, piece sacrifice initiative | King attack safety terms |
| **Conjuration** | 具現化 | **Structure** | Pawn structure integrity, passed pawns, outposts, fortresses | Pawn penalties & passed bonuses |
| **Specialization** | 特質 | **Creativity** | Imbalances (bishop pair, exchange sacrifices, unorthodox play) | Asymmetric piece imbalances |
| **Manipulation** | 操作 | **Prophylaxis** | Suffocating opponent counterplay, king shelter, prophylaxis | Shield & opponent mobility restriction |
| **Emission** | 放出 | **Activity** | Long-range piece activity, open/semi-open files, piece scope | Open file & long-range mobility |

### Calibrated Grandmaster Archetypes
Derived from statistical feature analysis over **150 tournament games (10,619 total plies)** across 6 legendary Grandmasters:

- **Mikhail Tal** (68% Calc, 70% Aggr, 40% Struct, 56% Creat, 58% Proph, 62% Act)
- **Tigran Petrosian** (25% Calc, 61% Aggr, 59% Struct, 61% Creat, 76% Proph, 79% Act)
- **Anatoly Karpov** (28% Calc, 79% Aggr, 30% Struct, 95% Creat, 81% Proph, 25% Act)
- **Paul Morphy** (95% Calc, 95% Aggr, 39% Struct, 25% Creat, 25% Proph, 67% Act)
- **Garry Kasparov** (50% Calc, 54% Aggr, 25% Struct, 85% Creat, 76% Proph, 90% Act)
- **Magnus Carlsen** (66% Calc, 25% Aggr, 95% Struct, 75% Creat, 95% Proph, 95% Act)
- **Balanced Nen** (50% on all traits)

For full mathematical definitions, mappings, and formulas, see [docs/nen_system.md](file:///Users/tanmay1301/projects/ChessEngine/docs/nen_system.md).

### Training Pipeline
The training pipeline processes PGN databases in `data/pgn/*.pgn` using `python-chess`:
```bash
# Install dependencies
pip install -r requirements.txt

# Run the training pipeline and derive nen_profiles.h
python3 scripts/train_nen_styles.py

# Verify committed header matches data derivation (CI check)
python3 scripts/train_nen_styles.py --check
```

## 🎯 Testing & Verification

The project includes an end-to-end automated test runner covering all engine and GUI components:

```bash
# Run master test suite (all 6 test suites)
python3 scripts/run_all_tests.py
```

This verifies:
1. **Nen Training CI Check** (`train_nen_styles.py --check`)
2. **Engine Core & Perft Benchmark Suite** (`test_engine.py`) — **18/18 perft benchmarks pass 100% green** (Startpos 1-5, Kiwipete 1-4, Pos 3 1-3, Pos 4 1-3, Pos 5 1-3), tactical puzzles, SEE, and self-play
3. **Nen Evaluation Behavioral Deltas** (`test_nen_eval.py`) — asserts positive cp deltas for Aggression (+16 cp) and Enhancement (+394 cp)
4. **UCI Protocol & Style/Nen Options** (`test_uci_nen.py`) — verifies option advertising and all 14 presets in a single session
5. **Game Flow Multi-Run Duplication Test** (`test_game_flow.py`) — validates no engine move duplication or corruption across 5 consecutive games
6. **GUI Server REST & SSE Endpoints** (`test_gui_server.py`) — verifies `/api/state`, `/api/style`, `/api/nen`, `/api/settings`, and `/api/fen`

## 📜 License

This project is open source. Feel free to use, modify, and distribute.
