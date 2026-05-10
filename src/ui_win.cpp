#include "ui_win.h"
#include "game.h"
#include "search.h"
#include "board.h"
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <cmath>

// ── global state ──────────────────────────────────────────────
static Game          gGame;
static HWND          gHwnd = nullptr;
static HMENU         gMenu = nullptr;

static std::thread   gAiThread;
static std::atomic<bool> gAiRunning{false};
static std::mutex    gMutex;
static Move          gLastAiMove;
static bool          gPlayerIsBlack = true;    // player = black by default
static int           gHoverR = -1, gHoverC = -1;
static bool          gGameOver = false;
static std::wstring  gStatus = L"你的回合 (● 黑棋)";

// ── forward / helper ──────────────────────────────────────────
static void startAiThink();
static void aiThinkFunc();
static void doNewGame();
static void updateMenu();
static void updateStatus();
static void paintBoard(HDC hdc);
static void paintStone(HDC hdc, int r, int c, int color, bool last);
static void getBoardPos(int mx, int my, int& r, int& c);

// ── menu IDs ──────────────────────────────────────────────────
enum {
    IDM_NEWGAME = 1001,
    IDM_SWITCH_SIDE,
    IDM_TIME_1S,
    IDM_TIME_3S,
    IDM_TIME_5S,
    IDM_TIME_10S,
    IDM_EXIT,
};

// ── AI thread ─────────────────────────────────────────────────
static void aiThinkFunc() {
    Board boardCopy;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        boardCopy = gGame.getBoard();
    }
    SearchResult res = gGame.engine().search(boardCopy);
    {
        std::lock_guard<std::mutex> lock(gMutex);
        gGame.applyMove(res.bestMove);
        gLastAiMove = res.bestMove;
        gAiRunning.store(false);
    }
    PostMessage(gHwnd, WM_APP, 0, 0);
}

static void startAiThink() {
    if (gAiRunning.load()) return;
    gAiRunning.store(true);
    gStatus = L"AI 思考中...";
    InvalidateRect(gHwnd, nullptr, FALSE);
    gAiThread = std::thread(aiThinkFunc);
    gAiThread.detach();
}

// ── game logic ────────────────────────────────────────────────
static void doNewGame() {
    if (gAiRunning.load()) return;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        gGame.reset();
        gLastAiMove = Move();
        gGameOver = false;
    }
    updateStatus();
    InvalidateRect(gHwnd, nullptr, TRUE);

    // AI goes first?
    if (!gPlayerIsBlack) {
        startAiThink();
    }
}

static void updateStatus() {
    if (gGameOver) {
        GameResult r = gGame.result();
        if (r == GameResult::BLACK_WIN)
            gStatus = (gPlayerIsBlack ? L"你赢了！(● 黑棋)" : L"AI 赢了！(● 黑棋)");
        else if (r == GameResult::WHITE_WIN)
            gStatus = (gPlayerIsBlack ? L"AI 赢了！(○ 白棋)" : L"你赢了！(○ 白棋)");
        else
            gStatus = L"平局！";
        return;
    }
    int side = gGame.getBoard().side();
    bool playerTurn = (gPlayerIsBlack && side == BLACK) ||
                      (!gPlayerIsBlack && side == WHITE);
    if (playerTurn)
        gStatus = (side == BLACK ? L"你的回合 (● 黑棋)" : L"你的回合 (○ 白棋)");
    else
        gStatus = L"AI 思考中...";
}

static void updateMenu() {
    if (!gMenu) return;
    MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
    mii.fMask = MIIM_STATE;
    mii.fState = MFS_UNCHECKED;
    SetMenuItemInfoW(gMenu, IDM_TIME_1S, FALSE, &mii);
    SetMenuItemInfoW(gMenu, IDM_TIME_3S, FALSE, &mii);
    SetMenuItemInfoW(gMenu, IDM_TIME_5S, FALSE, &mii);
    SetMenuItemInfoW(gMenu, IDM_TIME_10S, FALSE, &mii);
    UINT id;
    int t = gGame.engine().getConfig().timeMs;
    if (t <= 1500) id = IDM_TIME_1S;
    else if (t <= 4000) id = IDM_TIME_3S;
    else if (t <= 8000) id = IDM_TIME_5S;
    else id = IDM_TIME_10S;
    mii.fState = MFS_CHECKED;
    SetMenuItemInfoW(gMenu, id, FALSE, &mii);
}

// ── coordinate conversion ─────────────────────────────────────
static void getBoardPos(int mx, int my, int& r, int& c) {
    int col = (int)std::round((mx - UIWin::BOARD_LEFT) / (double)UIWin::CELL_SIZE);
    int row = (int)std::round((my - UIWin::BOARD_TOP)  / (double)UIWin::CELL_SIZE);
    if (col < 0) col = 0;
    if (col >= BOARD_SIZE) col = BOARD_SIZE - 1;
    if (row < 0) row = 0;
    if (row >= BOARD_SIZE) row = BOARD_SIZE - 1;
    // check if click is close enough to intersection
    int px = UIWin::BOARD_LEFT + col * UIWin::CELL_SIZE;
    int py = UIWin::BOARD_TOP  + row * UIWin::CELL_SIZE;
    int dx = mx - px, dy = my - py;
    if (dx*dx + dy*dy > UIWin::RADIUS * UIWin::RADIUS) {
        r = -1; c = -1;
    } else {
        r = row; c = col;
    }
}

// ── drawing ───────────────────────────────────────────────────
static void paintBoard(HDC hdc) {
    RECT rc;
    GetClientRect(gHwnd, &rc);

    // double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

    // background
    HBRUSH bgBrush = CreateSolidBrush(RGB(220, 180, 120));
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // board area slightly darker
    int bx = UIWin::BOARD_LEFT - 20, by = UIWin::BOARD_TOP - 20;
    int bw = UIWin::BOARD_PX + 40, bh = UIWin::BOARD_PX + 40;
    HBRUSH boardBg = CreateSolidBrush(RGB(210, 165, 100));
    RECT brc = { bx, by, bx + bw, by + bh };
    FillRect(memDC, &brc, boardBg);
    DeleteObject(boardBg);

    // grid lines
    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(60, 40, 20));
    HPEN oldPen = (HPEN)SelectObject(memDC, gridPen);
    for (int i = 0; i < BOARD_SIZE; ++i) {
        int pos = UIWin::BOARD_LEFT + i * UIWin::CELL_SIZE;
        MoveToEx(memDC, pos, UIWin::BOARD_TOP, nullptr);
        LineTo(memDC, pos, UIWin::BOARD_TOP + UIWin::BOARD_PX);
        MoveToEx(memDC, UIWin::BOARD_LEFT, pos, nullptr);
        LineTo(memDC, UIWin::BOARD_LEFT + UIWin::BOARD_PX, pos);
    }

    // star points (天元和星位)
    int stars[][2] = {{7,7}, {3,3}, {3,7}, {3,11}, {7,3}, {7,11}, {11,3}, {11,7}, {11,11}};
    HBRUSH starBrush = CreateSolidBrush(RGB(60, 40, 20));
    SelectObject(memDC, starBrush);
    SelectObject(memDC, GetStockObject(NULL_PEN));
    for (auto& s : stars) {
        int sx = UIWin::BOARD_LEFT + s[0] * UIWin::CELL_SIZE - 3;
        int sy = UIWin::BOARD_TOP  + s[1] * UIWin::CELL_SIZE - 3;
        Ellipse(memDC, sx, sy, sx + 7, sy + 7);
    }
    DeleteObject(starBrush);

    // stones
    const Board& board = gGame.getBoard();
    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c)
            if (board.get(r, c) != EMPTY)
                paintStone(memDC, r, c, board.get(r, c),
                           gLastAiMove.row == r && gLastAiMove.col == c);

    // hover preview
    if (gHoverR >= 0 && gHoverC >= 0 && !gGameOver && !gAiRunning.load()) {
        int side = board.side();
        bool playerTurn = (gPlayerIsBlack && side == BLACK) ||
                          (!gPlayerIsBlack && side == WHITE);
        if (playerTurn && board.isEmpty(gHoverR, gHoverC)) {
            int px = UIWin::BOARD_LEFT + gHoverC * UIWin::CELL_SIZE;
            int py = UIWin::BOARD_TOP  + gHoverR * UIWin::CELL_SIZE;
            HPEN dotPen = CreatePen(PS_DOT, 1, side == BLACK ? RGB(30,30,30) : RGB(220,220,220));
            SelectObject(memDC, dotPen);
            HBRUSH hovBrush = CreateSolidBrush(side == BLACK ? RGB(60,60,60) : RGB(240,240,240));
            SelectObject(memDC, hovBrush);
            Ellipse(memDC, px - UIWin::RADIUS, py - UIWin::RADIUS,
                    px + UIWin::RADIUS, py + UIWin::RADIUS);
            DeleteObject(hovBrush);
            DeleteObject(dotPen);
        }
    }

    // column labels
    SelectObject(memDC, GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(60, 40, 20));
    for (int i = 0; i < BOARD_SIZE; ++i) {
        int x = UIWin::BOARD_LEFT + i * UIWin::CELL_SIZE;
        wchar_t label[2] = { (wchar_t)(i < 10 ? L'0' + i : L'A' + i - 10), 0 };
        RECT lr = { x - 10, UIWin::BOARD_TOP - 30, x + 10, UIWin::BOARD_TOP - 10 };
        DrawTextW(memDC, label, -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        lr = { x - 10, UIWin::BOARD_TOP + UIWin::BOARD_PX + 10, x + 10, UIWin::BOARD_TOP + UIWin::BOARD_PX + 30 };
        DrawTextW(memDC, label, -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    // row labels
    for (int i = 0; i < BOARD_SIZE; ++i) {
        int y = UIWin::BOARD_TOP + i * UIWin::CELL_SIZE;
        wchar_t label[2] = { (wchar_t)(i < 10 ? L'0' + i : L'A' + i - 10), 0 };
        RECT lr = { UIWin::BOARD_LEFT - 30, y - 10, UIWin::BOARD_LEFT - 5, y + 10 };
        DrawTextW(memDC, label, -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        lr = { UIWin::BOARD_LEFT + UIWin::BOARD_PX + 5, y - 10, UIWin::BOARD_LEFT + UIWin::BOARD_PX + 30, y + 10 };
        DrawTextW(memDC, label, -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // title
    SetTextColor(memDC, RGB(40, 20, 0));
    RECT titleRC = { 0, 5, rc.right, 35 };
    DrawTextW(memDC, L"五子棋 AI  —  Alpha-Beta + VCT/VCF", -1, &titleRC, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // status bar
    RECT statusRC = { 0, rc.bottom - 35, rc.right, rc.bottom };
    HBRUSH statusBg = CreateSolidBrush(RGB(180, 140, 80));
    FillRect(memDC, &statusRC, statusBg);
    DeleteObject(statusBg);
    SetTextColor(memDC, RGB(40, 20, 0));
    DrawTextW(memDC, gStatus.c_str(), -1, &statusRC, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // blit
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBM);
    DeleteObject(gridPen);
    DeleteObject(memBM);
    DeleteDC(memDC);
}

static void paintStone(HDC hdc, int r, int c, int color, bool last) {
    int px = UIWin::BOARD_LEFT + c * UIWin::CELL_SIZE;
    int py = UIWin::BOARD_TOP  + r * UIWin::CELL_SIZE;
    int R  = UIWin::RADIUS;

    if (color == BLACK) {
        HBRUSH br = CreateSolidBrush(RGB(20, 20, 20));
        HPEN   pn = CreatePen(PS_SOLID, 1, RGB(10, 10, 10));
        SelectObject(hdc, br);
        SelectObject(hdc, pn);
        Ellipse(hdc, px - R, py - R, px + R, py + R);
        DeleteObject(br);
        DeleteObject(pn);
    } else {
        HBRUSH br = CreateSolidBrush(RGB(235, 235, 235));
        HPEN   pn = CreatePen(PS_SOLID, 2, RGB(30, 30, 30));
        SelectObject(hdc, br);
        SelectObject(hdc, pn);
        Ellipse(hdc, px - R, py - R, px + R, py + R);
        DeleteObject(br);
        DeleteObject(pn);
    }

    if (last) {
        // small red dot on last AI move
        HBRUSH dotBr = CreateSolidBrush(RGB(220, 50, 50));
        SelectObject(hdc, dotBr);
        SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, px - 4, py - 4, px + 4, py + 4);
        DeleteObject(dotBr);
    }
}

// ── window procedure ──────────────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {
        gHwnd = hwnd;
        // build menu
        HMENU gameMenu = CreateMenu();
        AppendMenuW(gameMenu, MF_STRING, IDM_NEWGAME, L"新游戏(&N)");
        AppendMenuW(gameMenu, MF_STRING, IDM_SWITCH_SIDE, L"切换先后手(&S)");
        AppendMenuW(gameMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(gameMenu, MF_STRING, IDM_EXIT, L"退出(&X)");

        HMENU setMenu = CreateMenu();
        AppendMenuW(setMenu, MF_STRING, IDM_TIME_1S, L"1 秒");
        AppendMenuW(setMenu, MF_STRING, IDM_TIME_3S, L"3 秒");
        AppendMenuW(setMenu, MF_STRING, IDM_TIME_5S, L"5 秒");
        AppendMenuW(setMenu, MF_STRING, IDM_TIME_10S, L"10 秒");

        HMENU bar = CreateMenu();
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)gameMenu, L"游戏(&G)");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)setMenu,  L"设置(&S)");
        SetMenu(hwnd, bar);
        gMenu = bar;

        updateMenu();
        {
            SearchConfig cfg = gGame.engine().getConfig();
            cfg.timeMs = 5000;
            gGame.engine().setConfig(cfg);
        }

        if (!gPlayerIsBlack) startAiThink();
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_NEWGAME:   doNewGame(); break;
        case IDM_SWITCH_SIDE:
            if (!gAiRunning.load()) {
                gPlayerIsBlack = !gPlayerIsBlack;
                doNewGame();
            }
            break;
        case IDM_TIME_1S:  { auto c = gGame.engine().getConfig(); c.timeMs = 1000;  gGame.engine().setConfig(c); } updateMenu(); break;
        case IDM_TIME_3S:  { auto c = gGame.engine().getConfig(); c.timeMs = 3000;  gGame.engine().setConfig(c); } updateMenu(); break;
        case IDM_TIME_5S:  { auto c = gGame.engine().getConfig(); c.timeMs = 5000;  gGame.engine().setConfig(c); } updateMenu(); break;
        case IDM_TIME_10S: { auto c = gGame.engine().getConfig(); c.timeMs = 10000; gGame.engine().setConfig(c); } updateMenu(); break;
        case IDM_EXIT:     DestroyWindow(hwnd); break;
        }
        return 0;

    case WM_APP:
        // AI finished thinking
        updateStatus();
        {
            std::lock_guard<std::mutex> lock(gMutex);
            GameResult r = gGame.result();
            if (r != GameResult::ONGOING) gGameOver = true;
        }
        updateStatus();
        InvalidateRect(hwnd, nullptr, TRUE);
        if (gGameOver) {
            MessageBoxW(hwnd, gStatus.c_str(), L"对局结束", MB_OK);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (gGameOver || gAiRunning.load()) return 0;
        {
            int side = gGame.getBoard().side();
            bool playerTurn = (gPlayerIsBlack && side == BLACK) ||
                              (!gPlayerIsBlack && side == WHITE);
            if (!playerTurn) return 0;
        }
        {
            int mx = LOWORD(lp), my = HIWORD(lp);
            int r, c;
            getBoardPos(mx, my, r, c);
            if (r < 0) return 0;
            {
                std::lock_guard<std::mutex> lock(gMutex);
                if (!gGame.makePlayerMove(r, c)) return 0;
                GameResult res = gGame.result();
                if (res != GameResult::ONGOING) gGameOver = true;
            }
            updateStatus();
            InvalidateRect(hwnd, nullptr, TRUE);
            if (gGameOver) {
                MessageBoxW(hwnd, gStatus.c_str(), L"对局结束", MB_OK);
            } else {
                startAiThink();
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        {
            int mx = LOWORD(lp), my = HIWORD(lp);
            int r, c;
            getBoardPos(mx, my, r, c);
            if (r != gHoverR || c != gHoverC) {
                gHoverR = r; gHoverC = c;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paintBoard(hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1; // skip — we paint everything

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ── UIWin ─────────────────────────────────────────────────────
UIWin::UIWin() {}

int UIWin::run(HINSTANCE hInstance, int nCmdShow) {
    // register window class
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"GomokuAIWindow";
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    // create window
    RECT wr = { 0, 0, WND_W, WND_H };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, TRUE);

    HWND hwnd = CreateWindowExW(
        0, L"GomokuAIWindow", L"五子棋 AI",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // message loop
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // cleanup
    if (gAiRunning.load()) {
        gGame.engine().stop();
        // give thread a moment
        Sleep(200);
    }

    return (int)msg.wParam;
}
