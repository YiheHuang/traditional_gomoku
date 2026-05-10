#pragma once

#include <string>
#include "board.h"
#include "game.h"
#include "search.h"

class UI {
public:
    UI();

    void run();

private:
    Game game;

    void drawBoard(const Board& board, Move lastAI) const;
    void printHeader() const;
    std::string stoneChar(int color) const;
    void showResult(GameResult r) const;

    Move parseMove(const std::string& input) const;
    bool isAIvsAI() const { return aiVsAI; }

    SearchConfig config;
    bool aiVsAI = false;
    int aivsaiDelayMs = 500;
};
