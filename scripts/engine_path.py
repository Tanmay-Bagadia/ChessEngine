"""
scripts/engine_path.py
Central engine executable path resolver.
Checks:
  1. $ENGINE_PATH environment variable
  2. ./build/engine
  3. ../build/engine
Raises RuntimeError if executable cannot be located.
"""

import os
import sys

def get_engine_path():
    candidates = []
    env_path = os.environ.get("ENGINE_PATH")
    if env_path:
        candidates.append(os.path.abspath(env_path))

    # Base directory relative paths
    curr_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(curr_dir, ".."))

    candidates.append(os.path.join(os.getcwd(), "build", "engine"))
    candidates.append(os.path.join(project_root, "build", "engine"))
    candidates.append(os.path.abspath(os.path.join(curr_dir, "..", "build", "engine")))

    for path in candidates:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return os.path.abspath(path)

    raise RuntimeError(
        f"Chess engine executable not found! Checked locations:\n"
        + "\n".join(f"  - {c}" for c in candidates)
        + "\nPlease compile the engine with 'cmake --build build' first."
    )

ENGINE_PATH = get_engine_path()
