cmake_minimum_required(VERSION 3.16)
project(gomoku LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── find pybind11 ─────────────────────────────────────────────
find_package(pybind11 REQUIRED)

# ── compile options ───────────────────────────────────────────
if(MSVC)
    add_compile_options(/O2 /W3 /utf-8)
else()
    add_compile_options(-O3 -Wall -Wextra)
endif()

# ── core C++ sources (shared between CLI and Python module) ──
set(CORE_SOURCES
    src/board.cpp
    src/pattern.cpp
    src/evaluator.cpp
    src/movegen.cpp
    src/transposition.cpp
    src/threat.cpp
    src/search.cpp
    src/game.cpp
)

# ── Python binding target ─────────────────────────────────────
pybind11_add_module(_gomoku_core MODULE
    ${CORE_SOURCES}
    src/bindings.cpp
)

target_include_directories(_gomoku_core PRIVATE src)

if(NOT MSVC)
    target_link_options(_gomoku_core PRIVATE -static)
endif()

# ── optional: also keep the Win32 GUI exe target ──────────────
# set(GUI_SOURCES ${CORE_SOURCES} src/ui_win.cpp src/resource.rc src/main.cpp)
# add_executable(gomoku WIN32 ${GUI_SOURCES})
# target_include_directories(gomoku PRIVATE src)
# if(MSVC)
#     target_link_libraries(gomoku gdi32)
# else()
#     target_link_libraries(gomoku gdi32 -mwindows -static)
# endif()
