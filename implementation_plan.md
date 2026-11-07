# Chess Engine: Self-Study Development Plan

A phased roadmap with **all the theory, documentation, tutorials, and reference code** you need to implement everything yourself.

---

## 🤝 Split-Responsibility Workflow

To ensure rapid engine development while mastering the core algorithms deeply:
- **Assistant (Antigravity):** Direct authoring of all boilerplate, string/FEN parsers, Perft & Divide test harnesses, UCI protocol command loops, build scripts (`CMakeLists.txt`), precomputed constant arrays, and roadmap tracking.
- **Learner (User):** Hands-on implementation of core algorithmic engines: `make_move()` (Steps 5–10), Zobrist incremental updates, `evaluate()`, `negamax()` / `alpha_beta()`, `quiescence()`, and `nnue` forward pass / accumulators.

---

## 🚀 Sprint Progress Tracker

| Milestone | Component | Owner | Status |
|---|---|---|---|
| **1.1** | Magic Bitboards & Slider Lookups | User / Pair | ✅ **Done** |
| **1.2** | Pseudo-Legal Move Generation (All pieces) | User / Pair | ✅ **Done** |
| **1.2b** | Stack-Allocated `MoveList` Optimization (256 moves) | Antigravity | ✅ **Done** |
| **1.3a** | `make_move()` Steps 1–4 (Captured piece shadowing fix, promo) | Antigravity | ✅ **Done** |
| **1.3b** | `make_move()` Steps 5–10 (Rook, EP, Rights, Occs, In-check, Side) | Antigravity | ✅ **Done** |
| **1.4** | Robust FEN Parser (`load_fen`, `START_FEN`, `KIWIPETE_FEN`) | Antigravity | ✅ **Done** |
| **1.9** | Perft & Divide Test Harness (`perft`, `perft_divide`, Suite) | Antigravity | ✅ **Done** |
| **1.10** | CMake Build System (`CMakeLists.txt`, Clang/GCC support) | Antigravity | ✅ **Done** |
| **1.5** | Zobrist Hashing (Incremental Hash Table & Keys) | Antigravity | ✅ **Done** |
| **1.6** | Hand-Crafted Evaluation (PeSTO PSTs, Material, Tapered) | Antigravity | ✅ **Done** |
| **1.7** | Search (Negamax, Alpha-Beta, Move Ordering, TT, Quiescence) | Antigravity | ✅ **Done** |
| **1.8** | UCI Protocol Command Loop (`uci`, `isready`, `position`, `go`) | Antigravity | ✅ **Done** |
| **Phase 2**| NNUE Architecture & Training Pipeline | Coached / Pair | ✅ **Done** |
| **Phase 3**| Analysis GUI & External Connectivity | Coached / Pair | 🔄 **CURRENT SPRINT** |

---

## 🏫 Master Resource Index

These are your primary references throughout the entire project. Bookmark them.

### Core References (Use Constantly)
| Resource | What It Is | Link |
|---|---|---|
| **Chess Programming Wiki** | The "bible" of chess engine development. Covers every topic. | [chessprogramming.org](https://www.chessprogramming.org/) |
| **TalkChess Forum** | Where top engine developers discuss techniques | [talkchess.com](http://www.talkchess.com/) |
| **Stockfish Source Code** | Gold-standard engine to study (advanced) | [github.com/official-stockfish/Stockfish](https://github.com/official-stockfish/Stockfish) |

### Video Tutorial Series (Follow Along)
| Series | Author | What It Covers | Link |
|---|---|---|---|
| **BBC Engine (Bit Board Chess)** | Code Monkey King | Complete bitboard engine from scratch in C (directly applicable to C++) | [YouTube Playlist](https://www.youtube.com/playlist?list=PLmN0neTso3Jxh8ZIylk74JpwfiWNI76Cs) / [GitHub](https://github.com/maksimKorzh/bbc) |
| **VICE Chess Engine** | Bluefever Software | Classic step-by-step engine tutorial in C | [YouTube Playlist](https://www.youtube.com/playlist?list=PLZ1QII7yudbc-Ky058TEaOstZHVbT-2hg) |
| **Coding Adventure: Chess** | Sebastian Lague | High-level visual introduction to all concepts | [YouTube](https://www.youtube.com/watch?v=U4ogK0MIzqk) |

### Reference Engines (Study Their Source Code)
| Engine | Language | Complexity | Best For | Link |
|---|---|---|---|---|
| **TSCP** | C | Beginner | Basic search + eval concepts | [tckerrigan.com](http://www.tckerrigan.com/Chess/TSCP/) |
| **Sunfish** | Python | Beginner | Understanding minimax/alpha-beta in ~111 lines | [github.com/thomasahle/sunfish](https://github.com/thomasahle/sunfish) |
| **BBC** | C | Intermediate | Bitboard movegen, magic bitboards | [github.com/maksimKorzh/bbc](https://github.com/maksimKorzh/bbc) |
| **Ethereal** | C | Advanced | Clean alpha-beta framework + NNUE transition | [github.com/AndyGrant/Ethereal](https://github.com/AndyGrant/Ethereal) |
| **Stockfish** | C++ | Expert | State-of-the-art everything | [github.com/official-stockfish/Stockfish](https://github.com/official-stockfish/Stockfish) |

---

## Phase 1: Complete the Engine Core

---

### 1.1 — Magic Bitboards

**What you're building:** A lookup table system that instantly returns all squares a rook/bishop can attack from any square, given any piece configuration on the board. Uses "magic" multiplication + right-shift to map occupancies to a perfect hash index.

#### Theory & Documentation
| Resource | Description |
|---|---|
| [CPW — Magic Bitboards](https://www.chessprogramming.org/Magic_Bitboards) | Definitive technical reference — how magic multiplication works, indexing scheme |
| [CPW — Looking for Magics](https://www.chessprogramming.org/Looking_for_Magics) | Algorithm to brute-force find magic numbers |
| [Magical Bitboards and How to Find Them](https://analog-hors.github.io/site/magic-bitboards/) | Excellent narrative blog post explaining the "why" and "how" from first principles |
| [Chess Move Generation with Magic Bitboards](https://rhysre.net/magic-bitboards.html) | Clear introduction bridging basic bitboards → magic hashing |

#### Video Tutorial
| Resource | Description |
|---|---|
| [Code Monkey King — Magic Bitboards](https://www.youtube.com/watch?v=k-a8uL5W0nE) | Step-by-step implementation you can code along with |

#### Key Concepts to Understand
1. **Occupancy mask** — which squares along a sliding piece's rays *matter* (exclude edges)
2. **Occupancy permutations** — all possible blocker configurations for a given mask (you already have `set_occupancy()`)
3. **Magic number** — a 64-bit constant that, when multiplied by the occupancy and shifted right, produces a unique index
4. **Attack table** — pre-computed array of attack bitboards indexed by `(magic * occupancy) >> shift`

#### Your existing code to build on
- `bishop_attacks_on_the_fly()` and `rook_attacks_on_the_fly()` in [movegen.cpp](file:///Users/tanmay1301/ChessEngine/engine_src/movegen.cpp) — these are your **reference implementations** for validating magic results
- `set_occupancy()` — already implemented for generating occupancy permutations
- `rook_masks[]` and `bishop_masks[]` — already populated by `init_sliders()`
- `rook_attacks[64][4096]` and `bishop_attacks[64][512]` — declared but **empty**, need to be filled

---

### 1.2 — Complete Move Generation

**What you're building:** Generate every pseudo-legal move for every piece type, both colors, including all special moves.

#### Theory & Documentation
| Resource | Description |
|---|---|
| [CPW — Move Generation](https://www.chessprogramming.org/Move_Generation) | Overview of pseudo-legal vs legal generation approaches |
| [CPW — Pawn Pushes](https://www.chessprogramming.org/Pawn_Push) | Bitboard pawn move patterns (you have white, need black) |
| [CPW — En Passant](https://www.chessprogramming.org/En_passant) | Tricky edge cases (discovered check through en passant!) |
| [CPW — Castling](https://www.chessprogramming.org/Castling) | Rights tracking, path checks, implementation |
| [CPW — Promotions](https://www.chessprogramming.org/Promotions) | Generate 4 promotion moves per promotion square |

#### What You Need to Implement (Checklist)
```
☑ Black pawn single pushes (>>8)
☑ Black pawn double pushes (>>16, from rank 7)
☑ Black pawn captures (>>7 east, >>9 west, with file masking)
☑ White pawn promotions (rank 8 pushes/captures → 4 moves each: Q/R/B/N)
☑ Black pawn promotions (rank 1)
☑ En passant captures (both colors)
☑ Knight moves: knight_attacks[sq] & ~friendly_occupancy
☑ Bishop moves: get_bishop_attacks(sq, occ) & ~friendly_occupancy
☑ Rook moves: get_rook_attacks(sq, occ) & ~friendly_occupancy
☑ Queen moves: (bishop | rook attacks) & ~friendly_occupancy
☑ King moves: king_attacks[sq] & ~friendly_occupancy
☑ Castling: check rights, empty squares, no attacks on path
☑ Stack-allocated MoveList (replaced heap std::vector)
```

#### Move Flag Encoding
Define these constants (you currently use magic numbers 0, 1, 4):
```cpp
enum MoveFlag {
    QUIET = 0,
    DOUBLE_PUSH = 1,
    KING_CASTLE = 2,
    QUEEN_CASTLE = 3,
    CAPTURE = 4,
    EP_CAPTURE = 5,
    KNIGHT_PROMO = 8,
    BISHOP_PROMO = 9,
    ROOK_PROMO = 10,
    QUEEN_PROMO = 11,
    KNIGHT_PROMO_CAPTURE = 12,
    BISHOP_PROMO_CAPTURE = 13,
    ROOK_PROMO_CAPTURE = 14,
    QUEEN_PROMO_CAPTURE = 15
};
```

---

### 1.3 — Make / Unmake Move

**What you're building:** Apply a move to the board (updating all bitboards, game state) and optionally reverse it.

#### Theory & Documentation
| Resource | Description |
|---|---|
| [CPW — Make Move](https://www.chessprogramming.org/Make_Move) | Both copy-make and incremental approaches |
| [CPW — Unmake Move](https://www.chessprogramming.org/Unmake_Move) | How to reverse a move using an undo stack |
| [CPW — Copy-Make](https://www.chessprogramming.org/Copy-Make) | Simpler approach — copy position before each move |

#### Implementation Strategy
> **Recommended: Copy-Make** — Copy the entire `Position` struct before making a move (`Position copy = pos;`). If the move is illegal (leaves king in check), `make_move` returns `false` and the copy is discarded. Simple, robust, and zero-overhead undo.

#### `make_move()` Implementation Step Breakdown

| Step | Task | Owner | Status |
|---|---|---|---|
| **Step 1** | Identify moving piece type & clear source bit | Antigravity | ✅ **Done** |
| **Step 2** | Clear captured piece on destination (`captured_piece`) | Antigravity | ✅ **Done** (Fixed shadowing bug) |
| **Step 3** | En Passant capture removal (destination `to ± 8`) | Antigravity | ✅ **Done** |
| **Step 4** | Place piece at destination (handles promotions & quiets) | Antigravity | ✅ **Done** |
| **Step 5** | Handle castling rook movement (`K_CASTLE`, `Q_CASTLE`) | User | 🔄 **TODO (Sprint)** |
| **Step 6** | Update en passant square (`DOUBLE_PUSH` vs `-1`) | User | 🔄 **TODO (Sprint)** |
| **Step 7** | Update castling rights with `castling_rights_mask[64]` | User | 🔄 **TODO (Sprint)** |
| **Step 8** | Recompute occupancies (`WHITE`, `BLACK`, `BOTH`) | User | 🔄 **TODO (Sprint)** |
| **Step 9** | King safety check (`is_square_attacked(king_sq, enemy, pos)`) | User | 🔄 **TODO (Sprint)** |
| **Step 10**| Switch side to move (`pos.side_to_move ^= 1`) | User | 🔄 **TODO (Sprint)** |

#### Coach's Step-by-Step Implementation Hints for Steps 5–10:
- **Step 5 (Castling Rook):**
  - White Kingside (`K_CASTLE`): King moved e1(4)→g1(6). Move rook from h1(7) to f1(5): `clear_bit(pos.pieces[WHITE][ROOK], 7); set_bit(pos.pieces[WHITE][ROOK], 5);`
  - White Queenside (`Q_CASTLE`): King moved e1(4)→c1(2). Move rook from a1(0) to d1(3): `clear_bit(pos.pieces[WHITE][ROOK], 0); set_bit(pos.pieces[WHITE][ROOK], 3);`
  - Black Kingside (`K_CASTLE`): King moved e8(60)→g8(62). Move rook from h8(63) to f8(61): `clear_bit(pos.pieces[BLACK][ROOK], 63); set_bit(pos.pieces[BLACK][ROOK], 61);`
  - Black Queenside (`Q_CASTLE`): King moved e8(60)→c8(58). Move rook from a8(56) to d8(59): `clear_bit(pos.pieces[BLACK][ROOK], 56); set_bit(pos.pieces[BLACK][ROOK], 59);`
- **Step 6 (En Passant Square):**
  - If `flag == DOUBLE_PUSH`: White moved rank 2→4 (`to - 8` is skipped sq), Black moved rank 7→5 (`to + 8` is skipped sq).
  - Else: set `pos.en_passant_square = -1`.
- **Step 7 (Castling Rights):**
  - Use the precomputed static mask:
    `pos.castling_rights &= castling_rights_mask[from];`
    `pos.castling_rights &= castling_rights_mask[to];`
- **Step 8 (Recompute Occupancies):**
  - Reset `pos.occupancy[WHITE] = pos.occupancy[BLACK] = 0ULL;`
  - Loop piece types 0..5: OR white pieces into `pos.occupancy[WHITE]`, black pieces into `pos.occupancy[BLACK]`.
  - `pos.occupancy[BOTH] = pos.occupancy[WHITE] | pos.occupancy[BLACK];`
- **Step 9 (Legality Check):**
  - Find king square of the side that JUST moved: `int king_sq = __builtin_ctzll(pos.pieces[side][KING]);`
  - If `is_square_attacked(king_sq, enemy, pos)`: return `false`! (King was in check).
- **Step 10 (Switch Side):**
  - `pos.side_to_move ^= 1;`
  - Return `true;` (legal move).

---

### 1.4 — FEN Parser [COMPLETED]

**What you're building:** Parse FEN strings to set up any chess position (not just the starting position).

#### Implementation Status: ✅ **Done**
- Implemented in `engine_src/position.h` and `engine_src/position.cpp` (`load_fen(Position &pos, const std::string &fen)`).
- Predefined constants: `START_FEN`, `KIWIPETE_FEN`.
- Parses piece placement, active side, castling rights (`KQkq`), en passant target square, halfmove clock, and fullmove counter.

---

### 1.5 — Zobrist Hashing

**What you're building:** A fast, incrementally-updateable hash function that gives each chess position a near-unique 64-bit fingerprint. Essential for the transposition table.

#### Theory & Documentation
| Resource | Description |
|---|---|
| [CPW — Zobrist Hashing](https://www.chessprogramming.org/Zobrist_Hashing) | Definitive reference — random number tables, XOR properties |
| [GeeksForGeeks — Zobrist Hashing](https://www.geeksforgeeks.org/minimax-algorithm-in-game-theory-set-5-zobrist-hashing/) | Step-by-step tutorial with code |
| [CPW — Transposition Table](https://www.chessprogramming.org/Transposition_Table) | How to use the hash in a search table |

#### The Key Insight
XOR is its own inverse: $A \oplus B \oplus B = A$

So to update the hash when a piece moves:
```cpp
hash ^= zobrist_table[piece][old_square];  // remove piece
hash ^= zobrist_table[piece][new_square];  // place piece
hash ^= zobrist_side;                       // flip side to move
```

#### Random Numbers Needed
- `U64 piece_keys[2][6][64]` — 768 random numbers (color × piece_type × square)
- `U64 side_key` — 1 random number
- `U64 castle_keys[16]` — 16 random numbers (one per castling combination)
- `U64 enpassant_keys[8]` — 8 random numbers (one per file)

---

### 1.6 — Evaluation Function (Hand-Crafted)

**What you're building:** A function that assigns a numerical score to any chess position. Positive = white is better, negative = black is better.

#### Theory & Documentation
| Resource | Description |
|---|---|
| [CPW — Evaluation](https://www.chessprogramming.org/Evaluation) | Complete overview of all evaluation concepts |
| [CPW — Piece-Square Tables](https://www.chessprogramming.org/Piece-Square_Tables) | Pre-computed bonuses/penalties per piece per square |
| [CPW — Tapered Eval](https://www.chessprogramming.org/Tapered_Eval) | Blending midgame and endgame scores — **essential technique** |
| [CPW — PeSTO](https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function) | Ready-to-use PST values that give ~2000 Elo with just material + PSTs |
| [CPW — Pawn Structure](https://www.chessprogramming.org/Pawn_Structure) | Doubled, isolated, passed, backward pawns |
| [CPW — King Safety](https://www.chessprogramming.org/King_Safety) | Pawn shield, attack counts, virtual mobility |

#### Implementation Order
1. **Material counting** — P=100, N=320, B=330, R=500, Q=900
2. **Piece-Square Tables** — use PeSTO values as a starting point
3. **Tapered eval** — interpolate between midgame and endgame PSTs
4. **Pawn structure** — doubled (-15), isolated (-20), passed (+30 to +90 by rank)
5. **Basic mobility** — count available squares for pieces
6. **King safety** — pawn shield bonus, open file penalty near king

#### Tapered Eval Formula
$$\text{score} = \frac{\text{mgScore} \times (256 - \text{phase}) + \text{egScore} \times \text{phase}}{256}$$

Where `phase` is calculated from remaining material (0 = pure opening, 256 = pure endgame).

---

### 1.7 — Search

**What you're building:** The "brain" of the engine — an algorithm that explores the game tree to find the best move.

#### Theory & Documentation

##### Tier 1 — Foundation (Start Here)
| Resource | Description |
|---|---|
| [CPW — Negamax](https://www.chessprogramming.org/Negamax) | Core algorithm: $\max(a,b) = -\min(-b,-a)$ |
| [CPW — Alpha-Beta](https://www.chessprogramming.org/Alpha-Beta) | Pruning branches that can't affect the result |
| [CPW — Iterative Deepening](https://www.chessprogramming.org/Iterative_Deepening) | Search depth 1, then 2, then 3... using previous results to improve ordering |
| [CPW — Quiescence Search](https://www.chessprogramming.org/Quiescence_Search) | Continue searching captures at leaf nodes to avoid "horizon effect" |
| [CPW — MVV-LVA](https://www.chessprogramming.org/MVV-LVA) | Simple capture ordering: prefer capturing high-value pieces with low-value pieces |

##### Tier 2 — Competitive Strength (Add After Tier 1 Works)
| Resource | Description |
|---|---|
| [CPW — Transposition Table](https://www.chessprogramming.org/Transposition_Table) | Cache search results, avoid re-searching same positions |
| [CPW — Killer Moves](https://www.chessprogramming.org/Killer_Move) | Remember quiet moves that caused cutoffs at each ply |
| [CPW — History Heuristic](https://www.chessprogramming.org/History_Heuristic) | Score quiet moves by how often they cause cutoffs historically |
| [CPW — Principal Variation Search](https://www.chessprogramming.org/Principal_Variation_Search) | Search first move fully, rest with null window — re-search if better |
| [CPW — Null Move Pruning](https://www.chessprogramming.org/Null_Move_Pruning) | Skip your turn and search shallower — if still good, prune |
| [CPW — Late Move Reductions](https://www.chessprogramming.org/Late_Move_Reductions) | Search moves late in the move list at reduced depth |
| [CPW — Aspiration Windows](https://www.chessprogramming.org/Aspiration_Windows) | Narrow the search window around the previous score guess |

##### Tier 3 — Advanced (After Engine Plays Well)
| Resource | Description |
|---|---|
| [CPW — Singular Extensions](https://www.chessprogramming.org/Singular_Extensions) | Extend search for the one clearly best move |
| [CPW — Static Exchange Evaluation](https://www.chessprogramming.org/Static_Exchange_Evaluation) | Evaluate capture sequences without searching them |
| [CPW — Futility Pruning](https://www.chessprogramming.org/Futility_Pruning) | Skip quiet moves when position is far below alpha |
| [CPW — Razoring](https://www.chessprogramming.org/Razoring) | Reduce depth when static eval is far below alpha |
| [CPW — Multi-Cut](https://www.chessprogramming.org/Multi-Cut) | Prune if multiple moves cause cutoffs at shallow depth |

#### Pseudocode Reference (Negamax + Alpha-Beta)
```cpp
int search(Position& pos, int alpha, int beta, int depth) {
    if (depth == 0) return quiescence(pos, alpha, beta);
    
    // TT lookup (Tier 2)
    // Null move pruning (Tier 2)
    
    std::vector<Move> moves = generate_moves(pos);
    order_moves(moves);  // MVV-LVA, then killers, then history
    
    for (Move move : moves) {
        Position copy = pos;     // copy-make
        make_move(copy, move);
        if (is_king_in_check(copy, !copy.side)) continue; // skip illegal
        
        int score = -search(copy, -beta, -alpha, depth - 1);
        
        if (score >= beta) return beta;    // beta cutoff
        if (score > alpha) alpha = score;  // new best
    }
    return alpha;
}
```

---

### 1.8 — UCI Protocol

**What you're building:** A text-based communication protocol so your engine can talk to any chess GUI (Arena, CuteChess, Lichess bots, etc.).

#### Specification & Documentation
| Resource | Description |
|---|---|
| [UCI Protocol Specification](https://www.shredderchess.com/chess-features/uci-chess-engine-protocol.html) | Official spec by Stefan Meyer-Kahlen — **read this end to end** |
| [UCI Protocol Full Text](http://wbec-ridderkerk.nl/html/UCIProtocol.html) | Alternative mirror of the complete protocol document |
| [CPW — UCI](https://www.chessprogramming.org/UCI) | Community wiki with implementation tips |

#### Minimal Commands to Implement
```
INPUT:  uci          → OUTPUT: id name YourEngine\n id author YourName\n uciok
INPUT:  isready      → OUTPUT: readyok
INPUT:  position startpos moves e2e4 e7e5 ...
INPUT:  position fen <fen> moves ...
INPUT:  go depth 10  → OUTPUT: info depth 10 score cp 35 nodes 12345 pv e2e4 e7e5 ...
                     → OUTPUT: bestmove e2e4
INPUT:  go wtime 60000 btime 60000 winc 1000 binc 1000
INPUT:  stop         → OUTPUT: bestmove <best>
INPUT:  quit
```

#### Time Management
| Resource | Description |
|---|---|
| [CPW — Time Management](https://www.chessprogramming.org/Time_Management) | How to allocate time per move |

Simple formula to start:
```cpp
int time_for_move = remaining_time / 30 + increment * 0.8;
```

---

### 1.9 — Perft Testing

**What you're building:** A function that counts all possible positions at a given depth. This is your **primary debugging tool** for move generation.

#### Theory & Documentation
| Resource | Description |
|---|---|
| [CPW — Perft](https://www.chessprogramming.org/Perft) | What perft is and how to implement it |
| [CPW — Perft Results](https://www.chessprogramming.org/Perft_Results) | Known-correct node counts for standard positions |
| [The Grand Chess Tree](https://grandchesstree.com/) | Community-computed perft results database |

#### Key Test Positions

**Starting Position:**
```
rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
```
| Depth | Nodes |
|---|---|
| 1 | 20 |
| 2 | 400 |
| 3 | 8,902 |
| 4 | 197,281 |
| 5 | 4,865,609 |
| 6 | 119,060,324 |

**Kiwipete (tests en passant, castling, promotions):**
```
r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
```
| Depth | Nodes |
|---|---|
| 1 | 48 |
| 2 | 2,039 |
| 3 | 97,862 |
| 4 | 4,085,603 |
| 5 | 193,690,690 |

#### Debugging Technique: Perft Divide
Split perft by root move to find which move has wrong count:
```
perft divide 3
a2a3: 380
a2a4: 420
b2b3: 420
...
Total: 8902
```
Compare each root move's count against Stockfish's output to isolate bugs.

#### Implementation Status: ✅ **Done**
- Harness files: `engine_src/perft.h` & `engine_src/perft.cpp`
- Implemented: `move_to_uci()` algebraic converter, recursive `perft()` node counter, `perft_divide()` per-move tree inspector, and automated `run_perft_suite()`.

---

### 1.10 — Build System [COMPLETED]

#### Implementation Status: ✅ **Done**
- Root build script: `CMakeLists.txt`
- Configured with C++17 standard, `-O3 -march=native` release optimizations, and automatic macOS Clang SDK include path resolution to eliminate libc++ header collisions with `/usr/local/include`.
- Build command:
  ```bash
  cmake -B build -S . && cmake --build build
  ./build/engine
  ```

---

## Phase 2: NNUE Training ("Train Like a Human")

---

### 2.1 — Understanding NNUE Architecture

#### Theory & Documentation
| Resource | Description |
|---|---|
| [CPW — NNUE](https://www.chessprogramming.org/NNUE) | Comprehensive overview — history, architecture, feature sets |
| [Stockfish NNUE Documentation](https://official-stockfish.github.io/docs/nnue.html) | Official docs — layers, quantization, incremental updates |
| [NNUEs and Where to Find Them](https://slama.dev/blog/nnu-es-and-where-to-find-them/) | Excellent practical walkthrough — implementation, quantization, output buckets |
| [Kaggle — Train Your Own Stockfish NNUE](https://www.kaggle.com/competitions/train-your-own-stockfish-nnue) | Hands-on competition with datasets and starter code |

#### NNUE Architecture Diagram
```
┌──────────────────────────────────────────┐
│  Input Features (HalfKP: 40960 sparse)   │
│  or Simple768 (768 sparse)               │
├──────────────────────────────────────────┤
│  Feature Transformer                      │
│  (40960 or 768) → 256, ClippedReLU       │
│  ★ Incrementally updated on make/unmake  │
├──────────────────────────────────────────┤
│  Perspective: concat white + black = 512  │
├──────────────────────────────────────────┤
│  Hidden 1: 512 → 32, ClippedReLU        │
├──────────────────────────────────────────┤
│  Hidden 2: 32 → 32, ClippedReLU         │
├──────────────────────────────────────────┤
│  Output: 32 → 1 (evaluation score)       │
└──────────────────────────────────────────┘
```

#### Feature Set Options
| Feature Set | Input Size | Concept | Recommended For |
|---|---|---|---|
| **Simple768** | 768 | 12 piece types × 64 squares | First NNUE implementation (simpler) |
| **HalfKP** | 40,960 | King square × piece × square, per side | Strong results, well-documented |
| **HalfKAv2** | ~50K | Latest Stockfish approach | Most complex, most powerful |

> [!TIP]
> **Start with Simple768** for your first NNUE. It's much easier to debug. Upgrade to HalfKP once it works.

### 2.2 — Training Tools & Frameworks

#### Option A: PyTorch (Recommended for Learning)
| Resource | Description |
|---|---|
| [official-stockfish/nnue-pytorch](https://github.com/official-stockfish/nnue-pytorch) | Stockfish's official PyTorch NNUE trainer — study this code |
| [PyTorch Tutorials](https://pytorch.org/tutorials/) | If you need to learn PyTorch basics first |

#### Option B: Bullet (High-Performance, Rust-Based)
| Resource | Description |
|---|---|
| [jw1912/bullet](https://github.com/jw1912/bullet) | Specialized NNUE trainer used by top engines — fastest option |

### 2.3 — Training Data

#### "Train Like a Human" Strategy
1. **Phase A — Learn from humans:** Train on high-Elo human games to capture human-like play patterns
2. **Phase B — Self-play refinement:** Use your engine to generate training data and retrain to push strength

#### Data Sources
| Source | Description | Link |
|---|---|---|
| **Lichess Open Database** | Millions of rated games, CC0 license, monthly PGN dumps | [database.lichess.org](https://database.lichess.org) |
| **Lichess Evaluations DB** | Stockfish evaluations for millions of positions (JSON) | Same page, "evaluations" section |
| **Kaggle Chess Datasets** | Pre-cleaned subsets for quick experiments | [kaggle.com](https://www.kaggle.com/datasets?search=lichess+chess) |

#### Data Processing Tools
| Tool | Description | Link |
|---|---|---|
| **python-chess** | Python library for PGN parsing, move generation, board manipulation | [python-chess.readthedocs.io](https://python-chess.readthedocs.io/) |
| **zstd** | Decompressor for Lichess `.pgn.zst` files | `pip install zstandard` |

#### Training Data Pipeline
```
Lichess PGN (2200+ games)
    │
    ▼  Parse with python-chess
    │
Positions + Game Results (W/D/L)
    │
    ▼  Label with Stockfish (depth 12+)
    │
Position Features + Eval Scores
    │
    ▼  Save as .binpack or .bin
    │
Ready for training
```

### 2.4 — NNUE Inference in C++

#### Key Concept: Incremental Updates
The feature transformer's output (the "accumulator") is large, but when a piece moves, only 1-2 features change. Instead of recomputing everything, you **add/subtract** individual feature columns:

```cpp
// When a white pawn moves from e2 to e4:
// Remove feature: (white_king_sq, white_pawn, e2)
accumulator -= feature_weights[feature_index(WK_sq, W_PAWN, E2)];
// Add feature: (white_king_sq, white_pawn, e4)
accumulator += feature_weights[feature_index(WK_sq, W_PAWN, E4)];
```

#### SIMD Acceleration
| Resource | Description |
|---|---|
| [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html) | Reference for AVX2/SSE instructions |
| [CPW — SIMD and SWAR Techniques](https://www.chessprogramming.org/SIMD_and_SWAR_Techniques) | Chess-specific SIMD patterns |

---

## Phase 3: Analysis GUI

---

### 3.1 — Technology Stack

#### Recommended: Web-based UI + Engine as Subprocess

| Component | Technology | Documentation |
|---|---|---|
| **Desktop Framework** | Tauri (lightweight) or Electron | [tauri.app/docs](https://tauri.app/) / [electronjs.org/docs](https://www.electronjs.org/docs) |
| **UI Framework** | React + Vite | [react.dev](https://react.dev/) / [vitejs.dev](https://vitejs.dev/) |
| **Chessboard UI** | chessground (Lichess's board) or chessboardjs | [github.com/lichess-org/chessground](https://github.com/lichess-org/chessground) / [chessboardjs.com](https://chessboardjs.com/) |
| **Chess Logic (JS)** | chess.js | [github.com/jhlywa/chess.js](https://github.com/jhlywa/chess.js) |
| **Engine Communication** | UCI over stdin/stdout pipes | Node.js `child_process.spawn()` |

#### Architecture
```
┌─────────────────────────────────┐
│  GUI (React + Chessground)       │
│  ├─ Board component              │
│  ├─ Eval bar component           │
│  ├─ Move list / PGN viewer       │
│  ├─ Multi-line analysis panel    │
│  └─ Eval graph (Chart.js/D3)    │
├─────────────────────────────────┤
│  Engine Bridge (Node.js)         │
│  ├─ Spawn engine subprocess      │
│  ├─ Send UCI commands             │
│  └─ Parse UCI info strings       │
├─────────────────────────────────┤
│  Your C++ Engine (UCI)           │
└─────────────────────────────────┘
```

#### Key Libraries to Study
| Library | What For | Link |
|---|---|---|
| **chessground** | The exact board that Lichess uses — drag/drop, arrows, highlights | [github.com/lichess-org/chessground](https://github.com/lichess-org/chessground) |
| **chess.js** | Chess logic in JS — move validation, PGN parsing, FEN | [github.com/jhlywa/chess.js](https://github.com/jhlywa/chess.js) |
| **Chart.js** or **D3.js** | For the evaluation graph over time | [chartjs.org](https://www.chartjs.org/) / [d3js.org](https://d3js.org/) |
| **PGN Parser** | For importing PGN files | Included in chess.js |

---

## Phase 4: Multi-Line Analysis (MultiPV)

---

### Engine Side

#### Theory
| Resource | Description |
|---|---|
| [CPW — Multi-PV](https://www.chessprogramming.org/Principal_Variation#MultiPV) | How to implement multiple principal variations |

#### UCI MultiPV Protocol
```
GUI  → setoption name MultiPV value 5
GUI  → go depth 20
ENGINE → info multipv 1 depth 20 score cp 45 pv d2d4 d7d5 c2c4 ...
ENGINE → info multipv 2 depth 20 score cp 32 pv e2e4 e7e5 ...
ENGINE → info multipv 3 depth 20 score cp 28 pv g1f3 d7d5 ...
ENGINE → bestmove d2d4
```

#### Implementation Approach
After finding PV 1, **exclude** that root move and search again to find PV 2, etc.

### GUI Side
- Parse `multipv` field from UCI info strings
- Display ranked list of lines with color-coded evaluations
- Draw semi-transparent arrows on the board for top N candidate moves (thicker/brighter = higher ranked)

---

## Phase 5: Polish & Compete

---

### Testing & Benchmarking Tools
| Tool | What For | Link |
|---|---|---|
| **cutechess-cli** | Automated engine vs engine matches with Elo calculation | [github.com/cutechess/cutechess](https://github.com/cutechess/cutechess) |
| **OpenBench** | Distributed testing framework for engine development | [github.com/AndyGrant/OpenBench](https://github.com/AndyGrant/OpenBench) |
| **SPRT (Sequential Probability Ratio Test)** | Statistical test to verify if a change is an improvement | [CPW — SPRT](https://www.chessprogramming.org/Match_Statistics#SPRT) |

### Performance Optimization
| Topic | Resource |
|---|---|
| **Lazy SMP** (multi-threaded search) | [CPW — Lazy SMP](https://www.chessprogramming.org/Lazy_SMP) |
| **PEXT Bitboards** (BMI2 instruction) | [CPW — BMI2 PEXT](https://www.chessprogramming.org/BMI2#PEXTBitboards) |
| **Profiling on macOS** | Use Instruments.app or `perf` |

---

## 📚 Recommended Reading Order

If you feel lost on where to start reading, follow this path:

```
1.  Sebastian Lague "Coding Adventure: Chess" (YouTube) — big picture
2.  Code Monkey King BBC Series (YouTube) — code along for bitboards + magics
3.  CPW — Magic Bitboards article — deep understanding
4.  CPW — Move Generation + Make Move — implement all pieces
5.  CPW — Perft Results — validate your movegen
6.  CPW — Negamax + Alpha-Beta — basic search
7.  CPW — Iterative Deepening + Quiescence — playable engine
8.  CPW — Transposition Table + Killer Moves — competitive search
9.  UCI Protocol Spec — connect to GUIs
10. CPW — NNUE + Stockfish NNUE docs — neural network eval
11. nnue-pytorch GitHub — training code reference
12. chessground docs — build the GUI
```

---

## Open Questions (Same as Before)

> [!IMPORTANT]
> **1. Position struct:** Refactor to `U64 pieces[2][6]` now? **(Strongly recommended: YES)**

> [!IMPORTANT]
> **2. GUI tech:** Tauri + React, Electron + React, or pure web app?

> [!IMPORTANT]
> **3. Move encoding:** Stay 16-bit or expand to 32-bit?

> [!WARNING]
> **4. First NNUE feature set:** Simple768 (easier) or HalfKP (more powerful)?

> [!NOTE]
> **5. Training data:** Start with Lichess human games (2200+) labeled by Stockfish? **(Recommended: YES)**
