#!/usr/bin/env python3
"""
Build helper: compile the C++/pybind11 extension and copy it into place.

Usage:
    python build_extension.py
    python build_extension.py --clean
"""

import subprocess
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).parent
BUILD_DIR = ROOT / "build"

def build():
    print("[1/3] Configuring CMake for MinGW...")
    subprocess.run([
        "cmake", "-B", str(BUILD_DIR),
        "-G", "MinGW Makefiles",
        "-DCMAKE_BUILD_TYPE=Release",
        "-Dpybind11_DIR=" +
        str(list(Path(sys.executable).parent.glob(
            "Lib/site-packages/pybind11/share/cmake/pybind11"))[0]),
    ], check=True, cwd=str(ROOT))

    print("[2/3] Building...")
    subprocess.run([
        "cmake", "--build", str(BUILD_DIR), "--config", "Release",
    ], check=True, cwd=str(ROOT))

    # Find and copy the .pyd
    pyd_files = list(BUILD_DIR.glob("_gomoku_core*.pyd"))
    if pyd_files:
        print(f"[3/3] Found: {pyd_files[0].name}")
        # Also leave a copy in the build directory for gomoku/__init__.py
        print("Done. Run: python main.py")
    else:
        print("ERROR: .pyd not found in build directory")
        sys.exit(1)

if __name__ == "__main__":
    build()
