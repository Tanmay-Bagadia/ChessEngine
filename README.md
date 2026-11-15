# ♟️ TanmayChess - Modern Chess Engine

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

### Evaluation
- **Tapered Evaluation** — Smooth interpolation between midgame and endgame
- **PeSTO Piece-Square Tables** — Well-tuned positional values
- **Pawn Structure** — Doubled, isolated, and passed pawn evaluation
- **Mobility** — Piece mobility bonuses for knights, bishops, rooks, queens
- **Bishop Pair Bonus** — Reward for having both bishops
- **Rook on Open/Semi-Open Files** — Bonus for rooks on files without friendly/enemy pawns
- **King Safety** — Pawn shield evaluation around the king
- **NNUE Support** — Optional neural network evaluation (768→256→32→32→1 architecture)

### UCI Protocol
- Full UCI protocol support
- `position`, `go`, `stop`, `quit`, `isready`, `ucinewgame`
- `setoption` for Hash, MultiPV, UseNNUE, EvalFile
- Custom commands: `d` (display), `eval`, `bench`, `fen`, `legalmoves`, `state`, `perftsuite`

### Web GUI
- Modern dark-themed interface (Lichess-inspired)
- Real-time engine analysis via Server-Sent Events
- Eval bar, MultiPV display, move history
- Play against engine, flip board, undo, custom FEN loading
- Promotion dialog, check/mate/stalemate detection

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

## 🎯 Testing

```bash
# Build and run perft suite
./build/engine
> perftsuite 4

# Run benchmark
> bench 8

# Python test suite (requires building first)
python3 scripts/test_engine.py
```

## 📜 License

This project is open source. Feel free to use, modify, and distribute.
