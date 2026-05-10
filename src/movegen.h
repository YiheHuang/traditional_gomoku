#pragma once

#include <vector>
#include <array>
#include "board.h"

class MoveGenerator {
public:
    MoveGenerator();

    // generate candidate moves sorted best-first
    std::vector<Move> generateMoves(const Board& board);

    // signal that this move caused a beta cutoff at this depth
    void addKiller(int depth, Move m);
    void addHistory(Move m, int depth);

    void clearKillers();
    void ageHistory();

    void sortMoves(std::vector<Move>& moves, const Board& board, int depth);

private:
    // killer[ply] stores two killer moves
    std::array<std::array<Move, 2>, 256> killers;
    // history[BOARD_CELLS] — history-heuristic score
    std::array<int, BOARD_CELLS> historyTable;

    int scoreMove(const Move& m, const Board& board, int depth) const;
};
