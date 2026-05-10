#include "board.h"
#include "pattern.h"
#include <random>

// --- static Zobrist tables ---
std::array<std::array<uint64_t, 3>, BOARD_CELLS> Board::zobristTable;
uint64_t Board::zobristSideToMove = 0;
bool Board::zobristReady = false;

void Board::initZobrist() {
    if (zobristReady) return;
    std::mt19937_64 rng(0x1A2B3C4D5E6F7089ULL);
    for (int i = 0; i < BOARD_CELLS; ++i) {
        for (int st = 0; st < 3; ++st) {
            zobristTable[i][st] = rng();
        }
    }
    zobristSideToMove = rng();
    zobristReady = true;
}

Board::Board() {
    initZobrist();
    reset();
}

void Board::reset() {
    for (int i = 0; i < BOARD_CELLS; ++i) cells[i] = EMPTY;
    sideToMove = BLACK;
    history.clear();
    zobristHash = 0;
}

bool Board::makeMove(int row, int col) {
    if (!inBounds(row, col)) return false;
    int idx = row * BOARD_SIZE + col;
    if (cells[idx] != EMPTY) return false;
    updateHash(idx, EMPTY, sideToMove);
    cells[idx] = sideToMove;
    history.push_back(Move(row, col));
    sideToMove = opponent(sideToMove);
    zobristHash ^= zobristSideToMove;
    return true;
}

void Board::undoMove() {
    if (history.empty()) return;
    Move m = history.back();
    history.pop_back();
    sideToMove = opponent(sideToMove);
    zobristHash ^= zobristSideToMove;
    int idx = m.index();
    updateHash(idx, cells[idx], EMPTY);
    cells[idx] = EMPTY;
}

void Board::updateHash(int idx, int oldSt, int newSt) {
    zobristHash ^= zobristTable[idx][oldSt];
    zobristHash ^= zobristTable[idx][newSt];
}

int Board::checkWinner() const {
    if (history.empty()) return EMPTY;
    Move last = history.back();
    int col = cells[last.index()];
    if (col == EMPTY) return EMPTY;

    for (int d = 0; d < DIR_NUM; ++d) {
        int dr = DIR_DR[d], dc = DIR_DC[d];
        int count = 1;
        // positive direction
        for (int r = last.row + dr, c = last.col + dc;
             inBounds(r, c) && get(r, c) == col; r += dr, c += dc) ++count;
        // negative direction
        for (int r = last.row - dr, c = last.col - dc;
             inBounds(r, c) && get(r, c) == col; r -= dr, c -= dc) ++count;
        if (count >= 5) return col;
    }
    return EMPTY;
}

bool Board::isFull() const {
    return ply() >= BOARD_CELLS;
}
