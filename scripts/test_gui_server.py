#!/usr/bin/env python3
"""
scripts/test_gui_server.py
Tests GUI server endpoints: /api/state, /api/style, /api/nen, /api/settings.
Order-independent and tests round-trip profile updates.
"""

import urllib.request
import urllib.error
import json
import time

BASE_URL = "http://localhost:3000"

def http_req(endpoint, method="GET", data=None):
    url = f"{BASE_URL}{endpoint}"
    payload = json.dumps(data).encode("utf-8") if data is not None else None
    headers = {"Content-Type": "application/json"} if data is not None else {}
    req = urllib.request.Request(url, data=payload, headers=headers, method=method)
    with urllib.request.urlopen(req, timeout=5) as resp:
        assert resp.status == 200, f"{method} {endpoint} returned {resp.status}"
        return json.loads(resp.read().decode("utf-8"))

def test_state_endpoint():
    print("=== Testing /api/state ===")
    state_data = http_req("/api/state", method="GET")
    assert "state" in state_data, "Missing 'state' in /api/state"
    assert "moves" in state_data, "Missing 'moves' in /api/state"
    assert "playSettings" in state_data, "Missing 'playSettings' in /api/state"
    assert "style" in state_data, "Missing 'style' in /api/state"
    assert "engineStatus" in state_data, "Missing 'engineStatus' in /api/state"
    print("  [PASS] /api/state structure verified!")

def test_style_and_nen_endpoints():
    print("=== Testing /api/style and /api/nen endpoints ===")
    # 1. GET /api/style and /api/nen
    style_get = http_req("/api/style", method="GET")
    assert "profile" in style_get and "preset" in style_get
    nen_get = http_req("/api/nen", method="GET")
    assert "profile" in nen_get and "preset" in nen_get

    # 2. POST /api/style with a specific trait
    post_res = http_req("/api/style", method="POST", data={
        "aggression": 88,
        "calculation": 72,
        "preset": "Custom"
    })
    assert post_res.get("ok") is True
    assert post_res["profile"]["aggression"] == 88
    # Aliases should sync
    assert post_res["profile"]["transmutation"] == 88
    assert post_res["profile"]["calculation"] == 72
    assert post_res["profile"]["enhancement"] == 72

    # 3. Verify persistence via GET /api/nen
    get_after = http_req("/api/nen", method="GET")
    assert get_after["profile"]["transmutation"] == 88
    assert get_after["profile"]["enhancement"] == 72
    assert get_after["preset"] == "Custom"

    # 4. POST preset update
    post_preset = http_req("/api/nen", method="POST", data={
        "preset": "Tal"
    })
    assert post_preset.get("ok") is True
    assert post_preset["preset"] == "Tal"

    # Restore Balanced
    http_req("/api/style", method="POST", data={"preset": "Balanced"})
    print("  [PASS] /api/style and /api/nen round-trip updates verified!")

def test_settings_endpoint():
    print("=== Testing /api/settings ===")
    res = http_req("/api/settings", method="POST", data={
        "playAgainstEngine": True,
        "playerColor": "w",
        "thinkTime": 250,
        "useNNUE": False
    })
    assert res.get("ok") is True
    assert res["playSettings"]["thinkTime"] == 250
    assert res["defaultOptions"]["useNNUE"] is False
    print("  [PASS] /api/settings verified!")

def main():
    test_state_endpoint()
    test_style_and_nen_endpoints()
    test_settings_endpoint()
    print("🎉 All GUI server tests PASSED!")

if __name__ == "__main__":
    main()
