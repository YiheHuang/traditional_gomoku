#pragma once

#include <string>
#include "board.h"
#include "search.h"

enum class GameResult { ONGOING, BLACK_WIN, WHITE_WIN, DRAW };

class Game {
public:
    Game();

    void reset();
    bool makePlayerMove(int row, int col);
    Move makeAIMove();
    void applyMove(Move m);

    const Board& getBoard() const { return board; }
    GameResult result() const;
    bool isOver() const;

    SearchEngine& engine() { return ai; }

    // AI plays black by default
    void setAIBlack(bool v) { aiIsBlack = v; }
    bool isAIBlack() const { return aiIsBlack; }

private:
    Board board;
    SearchEngine ai;
    bool aiIsBlack = true;
};
