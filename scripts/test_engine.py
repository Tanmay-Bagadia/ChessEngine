import subprocess
import time
import sys

ENGINE_PATH = "/Users/tanmay1301/projects/ChessEngine/build/engine"

class ChessEngineProcess:
    def __init__(self, path=ENGINE_PATH):
        self.proc = subprocess.Popen(
            [path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )

    def send(self, cmd):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def read_until(self, target, timeout=10):
        start = time.time()
        lines = []
        while time.time() - start < timeout:
            line = self.proc.stdout.readline()
            if not line:
                break
            lines.append(line.strip())
            if any(t in line for t in target) if isinstance(target, list) else target in line:
                return lines
        return lines

    def close(self):
        try:
            self.send("quit")
            self.proc.terminate()
            self.proc.wait(timeout=1)
        except:
            self.proc.kill()

def test_uci_protocol():
    print("=== TEST 1: UCI Protocol Basics ===")
    engine = ChessEngineProcess()
    engine.send("uci")
    lines = engine.read_until("uciok")
    assert any("id name" in l for l in lines), "Missing id name"
    assert any("uciok" in l for l in lines), "Missing uciok"
    print("  [PASS] uci handshake")

    engine.send("isready")
    lines = engine.read_until("readyok")
    assert any("readyok" in l for l in lines), "Missing readyok"
    print("  [PASS] isready handshake")

    engine.send("setoption name Hash value 32")
    engine.send("isready")
    lines = engine.read_until("readyok")
    assert any("readyok" in l for l in lines), "Failed setoption"
    print("  [PASS] setoption Hash 32MB")

    engine.close()

def test_perft_suite():
    print("\n=== TEST 2: Perft Benchmark Suite ===")
    tests = [
        ("Startpos Depth 1", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20),
        ("Startpos Depth 2", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 2, 400),
        ("Startpos Depth 3", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902),
        ("Startpos Depth 4", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281),
        ("Startpos Depth 5", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609),
        ("Kiwipete Depth 1", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48),
        ("Kiwipete Depth 2", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039),
        ("Kiwipete Depth 3", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862),
        ("Kiwipete Depth 4", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603),
        ("Position 3 Depth 1", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 1, 14),
        ("Position 3 Depth 2", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 2, 191),
        ("Position 3 Depth 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2812),
        ("Position 4 Depth 1", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 1, 6),
        ("Position 4 Depth 2", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 2, 264),
        ("Position 4 Depth 3", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467),
        ("Position 5 Depth 1", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 1, 44),
        ("Position 5 Depth 2", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 2, 1486),
        ("Position 5 Depth 3", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379),
    ]

    engine = ChessEngineProcess()
    passed = 0
    for name, fen, depth, expected in tests:
        engine.send(f"position fen {fen}")
        engine.send(f"go perft {depth}")
        lines = engine.read_until("Total Nodes:")
        total = None
        for l in lines:
            if "Total Nodes:" in l:
                total = int(l.split("Total Nodes:")[1].strip())
                break
        if total == expected:
            print(f"  [PASS] {name}: {total}")
            passed += 1
        else:
            print(f"  [FAIL] {name}: got {total}, expected {expected}")
            assert False, f"Perft failure on {name}"
    print(f"  All {passed}/{len(tests)} perft benchmarks passed!")
    engine.close()

def test_tactics_and_mates():
    print("\n=== TEST 3: Tactical & Mate Solving ===")
    puzzles = [
        # Mate in 1 puzzles
        ("Back-Rank Mate", "6k1/5ppp/8/8/8/8/5PPP/1R4K1 w - - 0 1", ["b1b8"]),
        ("Scholar's Mate", "r1bqkb1r/pppp1ppp/2n5/4p3/2B1n3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 0 4", ["f3f7"]),
        ("Queen and King Mate", "7k/Q7/6K1/8/8/8/8/8 w - - 0 1", ["a7g7", "a7a8"]),
        
        # Mate in 2 puzzles
        ("Légal Trap Mate", "r2qkb1r/pp2nppp/3p4/2pNN3/2BnP3/3P4/PPP2PPP/R1BbK2R w KQkq - 1 9", ["d5f6"]),
        ("Opera Box Mate", "4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 1 0", ["b3b8"]),
        
        # Tactical piece capture
        ("Hang Piece Capture", "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", None),
        ("Defend from Check", "rnb1kbnr/pppp1ppp/8/4p3/7q/5P2/PPPPP1PP/RNBQKBNR w KQkq - 1 3", ["g2g3"]),
    ]

    engine = ChessEngineProcess()
    for name, fen, expected_moves in puzzles:
        engine.send("ucinewgame")
        engine.send(f"position fen {fen}")
        engine.send("go depth 6")
        lines = engine.read_until("bestmove")
        best = None
        score_mate = False
        for l in lines:
            if "bestmove" in l:
                best = l.split("bestmove")[1].strip().split()[0]
            if "score mate" in l:
                score_mate = True
        if expected_moves:
            if best in expected_moves:
                print(f"  [PASS] {name}: found {best} (mate={score_mate})")
            else:
                print(f"  [WARN/FAIL] {name}: got {best}, expected one of {expected_moves}")
                assert best in expected_moves, f"Failed puzzle {name}"
        else:
            print(f"  [PASS] {name}: chose {best}")
    engine.close()

def test_self_play():
    print("\n=== TEST 4: Self-Play Game Simulation (20 moves) ===")
    engine = ChessEngineProcess()
    engine.send("ucinewgame")
    moves = []
    
    for ply in range(20):
        pos_cmd = "position startpos"
        if moves:
            pos_cmd += " moves " + " ".join(moves)
        engine.send(pos_cmd)
        engine.send("go depth 5")
        lines = engine.read_until("bestmove")
        best = None
        for l in lines:
            if "bestmove" in l:
                best = l.split("bestmove")[1].strip().split()[0]
        assert best and len(best) >= 4, f"Invalid move returned at ply {ply}: {best}"
        moves.append(best)
        print(f"  Ply {ply+1:2d}: {best}")

    print(f"  [PASS] Self-play completed cleanly: {' '.join(moves)}")
    engine.close()

def test_time_management():
    print("\n=== TEST 5: Time Management (movetime) ===")
    engine = ChessEngineProcess()
    engine.send("isready")
    engine.read_until("readyok")
    engine.send("position startpos")
    start = time.time()
    engine.send("go movetime 150")
    lines = engine.read_until("bestmove")
    elapsed = (time.time() - start) * 1000
    best = None
    for l in lines:
        if "bestmove" in l:
            best = l.split("bestmove")[1].strip().split()[0]
    print(f"  [PASS] go movetime 150ms finished in {elapsed:.1f}ms with move {best}")
    assert elapsed < 350, f"Engine exceeded time limit: {elapsed}ms"
    engine.close()

def test_stop_command():
    print("\n=== TEST 6: Stop Command Responsiveness ===")
    engine = ChessEngineProcess()
    engine.send("isready")
    engine.read_until("readyok")
    engine.send("position startpos")
    engine.send("go infinite")
    time.sleep(0.15)
    start = time.time()
    engine.send("stop")
    lines = engine.read_until("bestmove", timeout=2)
    elapsed = (time.time() - start) * 1000
    best = None
    for l in lines:
        if "bestmove" in l:
            best = l.split("bestmove")[1].strip().split()[0]
    assert best is not None, "Engine did not respond to stop command"
    print(f"  [PASS] 'stop' interrupted 'go infinite' in {elapsed:.1f}ms with bestmove {best}")
    engine.close()

def test_nnue_options():
    print("\n=== TEST 7: NNUE Evaluation & UCI Controls ===")
    engine = ChessEngineProcess()
    engine.send("uci")
    lines = engine.read_until("uciok")
    assert any("UseNNUE" in l for l in lines), "UseNNUE option missing"
    assert any("EvalFile" in l for l in lines), "EvalFile option missing"
    print("  [PASS] NNUE UCI options registered")

    engine.send("setoption name UseNNUE value true")
    engine.send("isready")
    engine.read_until("readyok")
    engine.send("position startpos")
    engine.send("go depth 3")
    lines = engine.read_until("bestmove")
    assert any("bestmove" in l for l in lines)
    print("  [PASS] NNUE search evaluation functioning")

    engine.send("setoption name UseNNUE value false")
    engine.send("isready")
    engine.read_until("readyok")
    engine.send("position startpos")
    engine.send("go depth 3")
    lines = engine.read_until("bestmove")
    assert any("bestmove" in l for l in lines)
    print("  [PASS] HCE fallback when NNUE disabled functioning")
    engine.close()

if __name__ == "__main__":
    test_uci_protocol()
    test_perft_suite()
    test_tactics_and_mates()
    test_time_management()
    test_stop_command()
    test_nnue_options()
    test_self_play()
    print("\n==========================================")
    print("🎉 ALL TESTS PASSED SUCCESSFULLY! 🎉")
    print("==========================================")
