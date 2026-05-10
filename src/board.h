#pragma once

#include <cstdint>
#include <vector>
#include <array>

constexpr int BOARD_SIZE = 15;
constexpr int BOARD_CELLS = BOARD_SIZE * BOARD_SIZE;

constexpr int EMPTY = 0;
constexpr int BLACK = 1;
constexpr int WHITE = 2;

inline int opponent(int color) { return color == BLACK ? WHITE : BLACK; }

struct Move {
    int row, col;
    Move() : row(-1), col(-1) {}
    Move(int r, int c) : row(r), col(c) {}
    bool valid() const { return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE; }
    int index() const { return row * BOARD_SIZE + col; }
    bool operator==(const Move& o) const { return row == o.row && col == o.col; }
    bool operator!=(const Move& o) const { return !(*this == o); }
};

class Board {
public:
    Board();

    void reset();
    bool makeMove(int row, int col);
    void undoMove();

    int get(int row, int col) const { return cells[row * BOARD_SIZE + col]; }
    int get(int idx) const { return cells[idx]; }
    bool isEmpty(int row, int col) const { return get(row, col) == EMPTY; }
    static bool inBounds(int row, int col) { return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE; }

    int side() const { return sideToMove; }
    int opp() const { return opponent(sideToMove); }
    int ply() const { return static_cast<int>(history.size()); }
    Move lastMove() const { return history.empty() ? Move() : history.back(); }

    uint64_t hash() const { return zobristHash; }

    const int* raw() const { return cells; }

    int checkWinner() const;
    bool isFull() const;

private:
    int cells[BOARD_CELLS];
    int sideToMove;
    std::vector<Move> history;
    uint64_t zobristHash;

    static std::array<std::array<uint64_t, 3>, BOARD_CELLS> zobristTable;
    static uint64_t zobristSideToMove;
    static bool zobristReady;

    static void initZobrist();
    void updateHash(int idx, int oldSt, int newSt);
};
