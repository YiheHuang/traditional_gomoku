#include "evaluator.h"
#include "pattern.h"
#include <cstring>

Evaluator::Evaluator() {}

int Evaluator::evaluate(const Board& board, int side) const {
    int winner = board.checkWinner();
    if (winner == side)  return 20000000;
    if (winner == opponent(side)) return -20000000;
    if (board.isFull()) return 0;

    int opp = opponent(side);

    // Track best attack / defence scores.
    int attBest[4] = {0, 0, 0, 0};
    int defBest[4] = {0, 0, 0, 0};

    const int* b = board.raw();

    // Only scan empty cells within Chebyshev distance 2 of any stone.
    bool near[BOARD_CELLS] = {false};
    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c)
            if (board.get(r, c) != EMPTY)
                for (int dr = -2; dr <= 2; ++dr)
                    for (int dc = -2; dc <= 2; ++dc) {
                        int nr = r + dr, nc = c + dc;
                        if (Board::inBounds(nr, nc) && board.isEmpty(nr, nc))
                            near[nr * BOARD_SIZE + nc] = true;
                    }

    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (!near[r * BOARD_SIZE + c]) continue;

            int att = quickScore(b, r, c, side);
            int def = quickScore(b, r, c, opp);

            if (att >= VAL_TWO || def >= VAL_TWO) {
                for (int i = 0; i < 4; ++i) {
                    if (att > attBest[i]) {
                        for (int j = 3; j > i; --j) attBest[j] = attBest[j - 1];
                        attBest[i] = att; break;
                    }
                }
                for (int i = 0; i < 4; ++i) {
                    if (def > defBest[i]) {
                        for (int j = 3; j > i; --j) defBest[j] = defBest[j - 1];
                        defBest[i] = def; break;
                    }
                }
            }
        }
    }

    // Add position bonus for existing stones.
    int posAtt = 0, posDef = 0;
    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c) {
            int stone = board.get(r, c);
            if (stone == side) posAtt += posBonus[r * BOARD_SIZE + c];
            else if (stone == opp) posDef += posBonus[r * BOARD_SIZE + c];
        }

    int attScore = attBest[0] + attBest[1] / 3 + attBest[2] / 9 + attBest[3] / 27 + posAtt;
    int defScore = defBest[0] + defBest[1] / 3 + defBest[2] / 9 + defBest[3] / 27 + posDef;

    if (defBest[0] >= VAL_FOUR) defScore = defScore * 5 / 3;

    return attScore - defScore;
}

int Evaluator::quickEval(const Board& board, int side) const {
    int winner = board.checkWinner();
    if (winner == side)  return 20000000;
    if (winner == opponent(side)) return -20000000;

    const int* b = board.raw();
    int opp = opponent(side);
    int maxAtt = 0, maxDef = 0;

    bool near[BOARD_CELLS] = {false};
    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c)
            if (board.get(r, c) != EMPTY)
                for (int dr = -2; dr <= 2; ++dr)
                    for (int dc = -2; dc <= 2; ++dc) {
                        int nr = r + dr, nc = c + dc;
                        if (Board::inBounds(nr, nc) && board.isEmpty(nr, nc))
                            near[nr * BOARD_SIZE + nc] = true;
                    }

    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (!near[r * BOARD_SIZE + c]) continue;
            int att = quickScore(b, r, c, side);
            int def = quickScore(b, r, c, opp);
            if (att > maxAtt) maxAtt = att;
            if (def > maxDef) maxDef = def;
        }
    }
    return maxAtt - maxDef;
}
