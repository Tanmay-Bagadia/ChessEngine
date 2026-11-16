# Nen Chess Playstyle System

## 1. Overview & Inspiration

The **Nen Playstyle System** models distinct chess personalities and tactical inclinations using the six Nen affinities from *Hunter × Hunter*. Rather than playing with a static, monolithic evaluation function, the engine dynamically modulates its evaluation weights across six fundamental chess dimensions.

Each affinity is mapped to specific terms in the C++ evaluation engine (`engine_src/evaluation.cpp`) and can be adjusted dynamically in real time from **0% to 100%** via UCI options (`Style_*` or `Nen_*`) or via the interactive web GUI.

```
                  [ Enhancement (強化) ]
                        Calculation
                             /\
               Activity     /  \    Aggression
        [ Emission (放出) ] /    \ [ Transmutation (変化) ]
                           |      |
                           |      |
             Prophylaxis   \      /   Structure
     [ Manipulation (操作) ]\    / [ Conjuration (具現化) ]
                             \/
                  [ Specialization (特質) ]
                         Creativity
```

---

## 2. The Six Nen Affinities & Chess Mapping

| Affinity (Nen) | Kanji | Chess Trait | Primary Evaluation Focus | Engine Parameters & Formula |
|---|---|---|---|---|
| **Enhancement** | 強化 | **Calculation** | Material valuation, piece values, tactical sharpness | Multiplies base material scores: `1.0 + (enhancement - 50) * 0.008` |
| **Transmutation** | 変化 | **Aggression** | King attack pressure, kingside storms, piece sacrifice initiative | Multiplies king safety attack terms: `1.0 + (aggression - 50) * 0.010` |
| **Conjuration** | 具現化 | **Structure** | Pawn structure integrity, passed pawns, outposts, defensive fortresses | Scales pawn penalties (doubled/isolated) & passed pawn bonuses: `1.0 + (structure - 50) * 0.008` |
| **Specialization** | 特質 | **Creativity** | Imbalances (bishop pair, exchange sacrifices, unorthodox material distribution) | Scales asymmetric piece bonuses and bishop pair values: `1.0 + (creativity - 50) * 0.008` |
| **Manipulation** | 操作 | **Prophylaxis** | Suffocating opponent counterplay, king shelter, prophylactic defense | Scales king pawn shield evaluation and opponent mobility restriction: `1.0 + (prophylaxis - 50) * 0.008` |
| **Emission** | 放出 | **Activity** | Long-range piece activity, open/semi-open files, board control | Scales rook file bonuses, diagonal mobility, and piece scope: `1.0 + (activity - 50) * 0.008` |

---

## 3. Grandmaster Archetypes & Calibration Data

The preset styles are not arbitrary numbers—they are derived from statistical feature analysis over **150 tournament games (10,619 total plies)** across six legendary Grandmasters:

1. **Mikhail Tal (The Magician from Riga)**: Dominant in **Transmutation & Enhancement** (high aggression, sacrifice for initiative, sharp calculation).
2. **Tigran Petrosian (Iron Tigran)**: Dominant in **Manipulation & Emission** (deep prophylaxis, suffocating counterplay, prophylactic piece activity).
3. **Anatoly Karpov**: Dominant in **Specialization & Manipulation** (asymmetric pressure, quiet prophylactic squeeze, small advantages).
4. **Paul Morphy**: Dominant in **Enhancement & Transmutation** (rapid calculation, decisive tactical breakthroughs, sharp king attacks).
5. **Garry Kasparov**: Dominant in **Emission & Specialization** (extreme dynamic activity, open files, dynamic piece imbalances).
6. **Magnus Carlsen**: Dominant in **Conjuration & Manipulation & Emission** (perfect pawn structure, prophylactic grind, maximizing total piece activity).
7. **Balanced Nen**: Neutral baseline with all affinities set to **50%**.

### Calibrated Trait Values

| Archetype | Calculation (Enh.) | Aggression (Trans.) | Structure (Conj.) | Creativity (Spec.) | Prophylaxis (Manip.) | Activity (Emis.) |
|---|---|---|---|---|---|---|
| **Mikhail Tal** | 68% | 70% | 40% | 56% | 58% | 62% |
| **Tigran Petrosian** | 25% | 61% | 59% | 61% | 76% | 79% |
| **Anatoly Karpov** | 28% | 79% | 30% | 95% | 81% | 25% |
| **Paul Morphy** | 95% | 95% | 39% | 25% | 25% | 67% |
| **Garry Kasparov** | 50% | 54% | 25% | 85% | 76% | 90% |
| **Magnus Carlsen** | 66% | 25% | 95% | 75% | 95% | 95% |
| **Balanced Nen** | 50% | 50% | 50% | 50% | 50% | 50% |

---

## 4. Training Pipeline & Feature Derivation

The training pipeline in `scripts/train_nen_styles.py` extracts position features from PGN datasets in `data/pgn/*.pgn`:
- `attack`: Attacks directed against enemy king zone.
- `pawn`: Pawn structure stability and passed pawn presence.
- `imbalance`: Piece count imbalances (bishops vs knights, rooks vs minors).
- `prophylaxis`: Shield integrity and opponent mobility restriction.
- `mobility`: Legal squares accessible to non-pawn pieces.
- `material`: Non-pawn pieces remaining on board.

### Normalization and Mapping Formula

Each feature is min-max normalized across all datasets:
$$\text{norm} = \frac{\text{feature} - \text{min}}{\text{max} - \text{min}}$$

The normalized feature is then scaled into a percentage value clamped between 25% and 95%:
$$\text{affinity} = \text{round}(25 + 70 \times \text{norm})$$

Running the pipeline automatically updates `engine_src/nen_profiles.h` with provenance metadata:
```bash
python3 scripts/train_nen_styles.py
```

To verify in CI that the committed header matches the data derivation:
```bash
python3 scripts/train_nen_styles.py --check
```

---

## 5. UCI Protocol Integration

The engine advertises both `Style_*` and `Nen_*` option aliases for compatibility:
- `setoption name Style_Calculation value 75` (or `setoption name Nen_Enhancement value 75`)
- `setoption name Style_Aggression value 85` (or `setoption name Nen_Transmutation value 85`)
- `setoption name Style_Structure value 60` (or `setoption name Nen_Conjuration value 60`)
- `setoption name Style_Creativity value 90` (or `setoption name Nen_Specialization value 90`)
- `setoption name Style_Prophylaxis value 80` (or `setoption name Nen_Manipulation value 80`)
- `setoption name Style_Activity value 70` (or `setoption name Nen_Emission value 70`)
- `setoption name Style_Preset value Tal` (or `Kasparov`, `Carlsen`, `Morphy`, `Karpov`, `Petrosian`, `Balanced`)

### NNUE Mode Interaction
When NNUE evaluation (`setoption name UseNNUE value true`) is active, the engine utilizes the neural network. If any style or Nen option is set, the engine automatically disables NNUE, emits:
```
info string Style options active, NNUE disabled (classical Nen eval)
```
and transitions to Classical Nen evaluation.
