"""
Gomoku AI - Python package powered by C++/pybind11 core.
"""

from pathlib import Path
import sys

# Find the compiled C++ extension:
#   1. PyInstaller bundle path (sys._MEIPASS)
#   2. Same directory as this package (for distributed builds)
#   3. Build directory (for development)
_package_dir = str(Path(__file__).parent)
_search_dirs = [_package_dir]

_meipass = getattr(sys, '_MEIPASS', None)
if _meipass:
    _search_dirs.insert(0, _meipass)

_build_dir = str(Path(__file__).parent.parent / "build")
_search_dirs.append(_build_dir)

for _d in _search_dirs:
    if _d not in sys.path:
        sys.path.insert(0, _d)

try:
    from _gomoku_core import *  # noqa: F401, F403
except ImportError:
    raise ImportError(
        "Cannot import _gomoku_core. Please build the C++ extension first:\n"
        "  cmake -B build -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release "
        "-Dpybind11_DIR=<path>\n"
        "  cmake --build build --config Release\n"
        "Then copy the .pyd to the gomoku/ directory."
    )
