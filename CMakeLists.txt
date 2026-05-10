cmake_minimum_required(VERSION 3.16)
project(gomoku LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/O2 /W3)
else()
    add_compile_options(-O3 -march=native -Wall -Wextra)
endif()

add_executable(gomoku WIN32
    src/board.cpp
    src/pattern.cpp
    src/evaluator.cpp
    src/movegen.cpp
    src/transposition.cpp
    src/threat.cpp
    src/search.cpp
    src/game.cpp
    src/ui_win.cpp
    src/main.cpp
)

target_include_directories(gomoku PRIVATE src)

if(MSVC)
    target_link_libraries(gomoku gdi32)
else()
    target_link_libraries(gomoku gdi32 -mwindows)
endif()
