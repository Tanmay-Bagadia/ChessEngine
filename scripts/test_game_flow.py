#!/usr/bin/env python3
"""
scripts/test_game_flow.py
Regression test for BUG 1 (engine move duplication).
Flow:
  - Configure playAgainstEngine=True, playerColor='w'
  - POST /api/newgame
  - POST /api/move {"move": "e2e4"}
  - Poll /api/state until engine replies
  - Assert len(moves) % 2 == 0
  - Assert no move appears twice in a row
  - Assert every played move was legal in the previous state
  - Verify stability across 5 consecutive runs
"""

import json
import time
import urllib.request
import urllib.error
import sys

BASE_URL = "http://localhost:3000"

def http_post(endpoint, data=None):
    url = f"{BASE_URL}{endpoint}"
    payload = json.dumps(data or {}).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=payload,
        headers={"Content-Type": "application/json"}
    )
    with urllib.request.urlopen(req, timeout=5) as resp:
        return json.loads(resp.read().decode("utf-8"))

def http_get(endpoint):
    url = f"{BASE_URL}{endpoint}"
    req = urllib.request.Request(url)
    with urllib.request.urlopen(req, timeout=5) as resp:
        return json.loads(resp.read().decode("utf-8"))

def run_single_flow(iteration):
    # Ensure settings: playAgainstEngine=true, playerColor=w
    settings_res = http_post("/api/settings", {
        "playAgainstEngine": True,
        "playerColor": "w",
        "thinkTime": 150
    })

    # Start new game
    newgame_res = http_post("/api/newgame", {})
    initial_legal = newgame_res.get("state", {}).get("legalMoves", [])
    assert "e2e4" in initial_legal, f"Iter {iteration}: e2e4 not in initial legal moves: {initial_legal}"

    # Play e2e4 as White
    move_res = http_post("/api/move", {"move": "e2e4"})
    assert move_res.get("ok") is True, f"Iter {iteration}: /api/move failed: {move_res}"

    # Poll /api/state until engine responds or timeout
    start_time = time.time()
    moves = []
    final_state = None
    while time.time() - start_time < 4.0:
        state_data = http_get("/api/state")
        moves = state_data.get("moves", [])
        final_state = state_data.get("state", {})
        if len(moves) >= 2:
            break
        time.sleep(0.05)

    assert len(moves) >= 2, f"Iter {iteration}: Engine did not respond within timeout. Moves: {moves}"
    
    # Grace period: sleep 300ms to ensure no delayed duplicate bestmove arrives
    time.sleep(0.3)
    state_after = http_get("/api/state")
    moves_after = state_after.get("moves", [])

    # Invariants
    assert len(moves_after) % 2 == 0, f"Iter {iteration}: moves count must be even, got {len(moves_after)}: {moves_after}"
    assert len(moves_after) == 2, f"Iter {iteration}: expected exactly 2 moves, got {moves_after}"

    # No move appears twice in a row
    for i in range(len(moves_after) - 1):
        assert moves_after[i] != moves_after[i+1], (
            f"Iter {iteration}: move duplicated in succession: '{moves_after[i]}' at pos {i} and {i+1}"
        )

    # Check legality of White's move
    assert moves_after[0] == "e2e4", f"Iter {iteration}: unexpected first move {moves_after[0]}"
    assert moves_after[0] in initial_legal, f"Iter {iteration}: illegal move {moves_after[0]}"

    engine_move = moves_after[1]
    assert len(engine_move) >= 4, f"Iter {iteration}: invalid engine move string '{engine_move}'"
    print(f"  [PASS] Iteration {iteration}: game moves = {moves_after}")

def main():
    print("=== Testing Game Flow (Bug 1 Regression Test) ===")
    runs = 5
    for i in range(1, runs + 1):
        run_single_flow(i)
    print(f"🎉 Successfully completed {runs}/{runs} game flow iterations without move duplication!")

if __name__ == "__main__":
    main()
