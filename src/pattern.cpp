#include "pattern.h"
#include "board.h"

DirResult analyseDir(const int* board, int r, int c, int dr, int dc, int colour) {
    DirResult res;
    // count consecutive same-colour stones (not counting the target cell itself)
    int cnt = 0;
    int open = 0;

    // +direction
    int nr = r + dr, nc = c + dc;
    while (Board::inBounds(nr, nc) && board[nr * BOARD_SIZE + nc] == colour) {
        ++cnt;
        nr += dr; nc += dc;
    }
    if (Board::inBounds(nr, nc) && board[nr * BOARD_SIZE + nc] == EMPTY) ++open;

    // -direction
    nr = r - dr; nc = c - dc;
    while (Board::inBounds(nr, nc) && board[nr * BOARD_SIZE + nc] == colour) {
        ++cnt;
        nr -= dr; nc -= dc;
    }
    if (Board::inBounds(nr, nc) && board[nr * BOARD_SIZE + nc] == EMPTY) ++open;

    res.count = cnt + 1;  // +1 for the stone we would place
    res.openEnds = open;
    return res;
}

int patternScore(const DirResult& d) {
    if (d.count >= 5) return VAL_FIVE;
    if (d.count == 4) {
        if (d.openEnds == 2) return VAL_OPEN_FOUR;
        if (d.openEnds == 1) return VAL_FOUR;
    }
    if (d.count == 3) {
        if (d.openEnds == 2) return VAL_OPEN_THREE;
        if (d.openEnds == 1) return VAL_THREE;
    }
    if (d.count == 2) {
        if (d.openEnds == 2) return VAL_OPEN_TWO;
        if (d.openEnds == 1) return VAL_TWO;
    }
    if (d.count == 1) {
        if (d.openEnds == 2) return VAL_OPEN_ONE;
        if (d.openEnds == 1) return VAL_ONE;
    }
    return 0;
}

int compoundBonus(const DirResult dr[DIR_NUM]) {
    int fours = 0, openThrees = 0, threes = 0;
    for (int i = 0; i < DIR_NUM; ++i) {
        if (dr[i].count >= 5) return VAL_FIVE;
        if (dr[i].count == 4) {
            if (dr[i].openEnds == 2) return VAL_OPEN_FOUR;
            if (dr[i].openEnds == 1) ++fours;
        }
        if (dr[i].count == 3) {
            if (dr[i].openEnds == 2) ++openThrees;
            if (dr[i].openEnds == 1) ++threes;
        }
    }

    // Double-four (or four + open-three) is a forced win
    if (fours >= 2) return VAL_DOUBLE_FOUR;
    if (fours >= 1 && openThrees >= 1) return VAL_DOUBLE_FOUR; // four+open_three also wins

    // Double open-three is a forced win
    if (openThrees >= 2) return VAL_DOUBLE_THREE;

    // Four + three is strong but not instant win
    if (fours >= 1 && threes >= 1) return VAL_OPEN_THREE;

    return 0;
}

int quickScore(const int* board, int r, int c, int colour) {
    int score = 0;
    DirResult dr[DIR_NUM];
    for (int d = 0; d < DIR_NUM; ++d) {
        dr[d] = analyseDir(board, r, c, DIR_DR[d], DIR_DC[d], colour);
        int s = patternScore(dr[d]);
        if (s >= VAL_OPEN_FOUR) return s; // early exit for killer threats
        score += s;
    }
    score += compoundBonus(dr);
    return score;
}
