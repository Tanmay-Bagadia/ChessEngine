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

## ♟️ Dynamic Playstyle Settings

The engine modulates its classical evaluation weights across six core chess dimensions configurable via sliders, presets, or UCI options:

| Chess Setting | UCI Option | Primary Evaluation Focus | Multiplier Scaling |
|---|---|---|---|
| **Calculation** | `Style_Calculation` | Material valuation, piece tension, tactical precision | Base material scores |
| **Aggression** | `Style_Aggression` | King attack pressure, kingside storms, sacrifices | King attack safety terms |
| **Structure** | `Style_Structure` | Pawn chains, passed pawns, outposts, fortresses | Pawn penalties & passed bonuses |
| **Creativity** | `Style_Creativity` | Asymmetric piece imbalances (bishop pair, exchanges) | Asymmetric piece imbalances |
| **Defense** | `Style_Defense` | King pawn shelter, prophylaxis, opponent restriction | King shield & defense terms |
| **Activity** | `Style_Activity` | Long-range piece mobility, open/semi-open files | Open file & piece mobility |

### Prime-Era Grandmaster Profiles
Derived empirically from **900 prime-era classical tournament games (150 per player, 30,778 GM-perspective middlegame/endgame plies)** across 6 legendary World Champions & masters:

- **Mikhail Tal (1957–1961)** — 61% Calc, 56% Aggr, 43% Struct, 79% Creat, 62% Def, 80% Act
- **Tigran Petrosian (1962–1969)** — 31% Calc, 25% Aggr, 95% Struct, 25% Creat, 95% Def, 25% Act
- **Anatoly Karpov (1974–1985)** — 25% Calc, 31% Aggr, 85% Struct, 63% Creat, 93% Def, 64% Act
- **Paul Morphy (1857–1859)** — 95% Calc, 95% Aggr, 25% Struct, 95% Creat, 25% Def, 95% Act
- **Garry Kasparov (1985–2000)** — 49% Calc, 48% Aggr, 65% Struct, 56% Creat, 92% Def, 69% Act
- **Magnus Carlsen (2013–2021)** — 54% Calc, 43% Aggr, 27% Struct, 43% Creat, 70% Def, 78% Act
- **Balanced** — 50% on all traits

### Training Pipelines
```bash
# Install dependencies
pip install -r requirements.txt

# Re-derive prime-era Grandmaster style profiles into engine_src/nen_profiles.h
python3 scripts/train_nen_styles.py
python3 scripts/train_nen_styles.py --check

# Train quantized NNUE v2 (768->256->32->32->1) on 900 prime GM games + augmentation (170k+ positions)
python3 nnue_trainer/train.py
```

## 🎯 Testing & Verification

The project includes an end-to-end automated test runner covering all engine, NNUE, and GUI components:

```bash
# Run master test suite (all 7 test suites)
python3 scripts/run_all_tests.py
```

This verifies:
1. **Playstyle Training CI Check** (`train_nen_styles.py --check`)
2. **Engine Core & Perft Benchmark Suite** (`test_engine.py`) — **18/18 perft benchmarks pass 100% green**, tactical puzzles, SEE, and self-play
3. **NNUE v2 Accuracy & Symmetry** (`test_nnue_accuracy.py`) — verifies accurate centipawn scaling, White/Black perspective symmetry, and fallback path loading
4. **Playstyle Evaluation Behavioral Deltas** (`test_nen_eval.py`) — asserts positive cp deltas for Aggression and Calculation
5. **UCI Protocol & Style Options** (`test_uci_nen.py`) — verifies option advertising and all presets in a single session
6. **Game Flow Multi-Run Duplication Test** (`test_game_flow.py`) — validates no engine move duplication across 5 consecutive games
7. **GUI Server REST & SSE Endpoints** (`test_gui_server.py`) — verifies `/api/state`, `/api/style`, `/api/settings`, and `/api/fen`

## 📜 License

This project is open source. Feel free to use, modify, and distribute.
