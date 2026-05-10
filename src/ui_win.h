#pragma once

#include <windows.h>

class UIWin {
public:
    // default window size (actual layout computed dynamically)
    static constexpr int DEF_WND_W = 640;
    static constexpr int DEF_WND_H = 700;

    UIWin();
    int run(HINSTANCE hInstance, int nCmdShow);

private:
    UIWin(const UIWin&) = delete;
    UIWin& operator=(const UIWin&) = delete;
};
