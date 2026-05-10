#pragma once

#include <cstdint>

// Direction offsets: (dr, dc) for E, S, SE, SW
constexpr int DIR_NUM = 4;
constexpr int DIR_DR[4] = { 0, 1, 1,  1 };
constexpr int DIR_DC[4] = { 1, 0, 1, -1 };

// --- pattern types ---
enum Pattern {
    P_NONE = 0,
    P_DEAD,
    P_ONE,
    P_TWO,
    P_THREE,
    P_FOUR,
    P_OPEN_ONE,
    P_OPEN_TWO,
    P_OPEN_THREE,
    P_OPEN_FOUR,
    P_DOUBLE_THREE,    // two open-threes meeting at one cell
    P_DOUBLE_FOUR,     // two fours meeting at one cell
    P_FIVE,
};

// scores (AI side positive, opponent side negative)
constexpr int VAL_FIVE        =  10000000;
constexpr int VAL_OPEN_FOUR   =   1000000;
constexpr int VAL_DOUBLE_FOUR =    500000;
constexpr int VAL_FOUR        =    100000;
constexpr int VAL_DOUBLE_THREE=     50000;
constexpr int VAL_OPEN_THREE  =     10000;
constexpr int VAL_THREE       =      1000;
constexpr int VAL_OPEN_TWO    =       200;
constexpr int VAL_TWO         =        20;
constexpr int VAL_OPEN_ONE    =         5;
constexpr int VAL_ONE         =         1;

// per-cell pattern record for one colour in one direction
struct DirResult {
    int count = 0;      // consecutive same-colour stones after placing here (1..5+)
    int openEnds = 0;   // 0..2
};

// Analyse one direction for a cell and colour.
// board[BOARD_CELLS], 0=empty, 1=black, 2=white.
DirResult analyseDir(const int* board, int r, int c, int dr, int dc, int colour);

// Classify a DirResult into a Pattern, and return its score.
int patternScore(const DirResult& d);

// Detect compound patterns (double-three / double-four) by combining
// the four direction results.
int compoundBonus(const DirResult dr[DIR_NUM]);

// Quick score of placing `colour` at (r,c) — used for move ordering.
// `board` must be a flat 15x15 array.
int quickScore(const int* board, int r, int c, int colour);
