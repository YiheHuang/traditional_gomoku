"""
Gomoku AI - Python package powered by C++/pybind11 core.
"""

from pathlib import Path
import sys

# Auto-detect and load the compiled C++ extension
_BUILD_DIR = Path(__file__).parent.parent / "build"
if str(_BUILD_DIR) not in sys.path:
    sys.path.insert(0, str(_BUILD_DIR))

try:
    from _gomoku_core import *  # noqa: F401, F403
except ImportError:
    raise ImportError(
        "Cannot import _gomoku_core. Please build the C++ extension first:\n"
        "  cmake -B build -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release "
        "-Dpybind11_DIR=<path>\n"
        "  cmake --build build --config Release"
    )
