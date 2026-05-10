#pragma once

#include <windows.h>

class UIWin {
public:
    static constexpr int CELL_SIZE = 36;
    static constexpr int BOARD_LEFT = 50;
    static constexpr int BOARD_TOP = 70;
    static constexpr int BOARD_PX = CELL_SIZE * 14;
    static constexpr int RADIUS = 16;
    static constexpr int WND_W = BOARD_LEFT * 2 + BOARD_PX;
    static constexpr int WND_H = BOARD_TOP + BOARD_PX + 60;

    UIWin();
    int run(HINSTANCE hInstance, int nCmdShow);

private:
    UIWin(const UIWin&) = delete;
    UIWin& operator=(const UIWin&) = delete;
};
