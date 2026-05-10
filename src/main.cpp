#include "ui_win.h"
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    UIWin ui;
    return ui.run(hInstance, nCmdShow);
}
