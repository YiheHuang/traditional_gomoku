#include "ui.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>

UI::UI() {
    config.timeMs = 5000;
    config.useVCT = true;
    config.useTT = true;
    config.useKiller = true;
    config.useHistory = true;
    config.vctDepth = 15;
    game.engine().setConfig(config);
}

void UI::printHeader() const {
    std::cout << "\n"
              << "  ╔══════════════════════════════════════╗\n"
              << "  ║        五子棋 AI - Alpha-Beta       ║\n"
              << "  ║    15x15 棋盘 · VCT/VCF 威胁搜索    ║\n"
              << "  ╚══════════════════════════════════════╝\n\n";
}

std::string UI::stoneChar(int color) const {
    if (color == BLACK) return "●";
    if (color == WHITE) return "○";
    return "·";
}

void UI::drawBoard(const Board& board, Move lastAI) const {
    std::cout << "\n   ";
    for (int c = 0; c < BOARD_SIZE; ++c) {
        if (c < 10) std::cout << " " << c << " ";
        else         std::cout << " " << (char)('A' + c - 10) << " ";
    }
    std::cout << "\n";

    for (int r = 0; r < BOARD_SIZE; ++r) {
        if (r < 10) std::cout << " " << r << " ";
        else        std::cout << " " << (char)('A' + r - 10) << " ";
        for (int c = 0; c < BOARD_SIZE; ++c) {
            int stone = board.get(r, c);
            if (lastAI.valid() && r == lastAI.row && c == lastAI.col) {
                std::cout << "[";
            } else {
                std::cout << " ";
            }
            std::cout << stoneChar(stone);
            if (lastAI.valid() && r == lastAI.row && c == lastAI.col) {
                std::cout << "]";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

void UI::showResult(GameResult r) const {
    std::cout << "\n════════════════════════════\n";
    if (aiVsAI) {
        if (r == GameResult::BLACK_WIN) std::cout << "  黑棋 (AI) 获胜！\n";
        else if (r == GameResult::WHITE_WIN) std::cout << "  白棋 (AI) 获胜！\n";
        else std::cout << "  平局！\n";
    } else {
        if (r == GameResult::BLACK_WIN) {
            std::cout << (game.isAIBlack() ? "  AI (黑棋) 获胜！" : "  你 (黑棋) 获胜！") << "\n";
        } else if (r == GameResult::WHITE_WIN) {
            std::cout << (game.isAIBlack() ? "  你 (白棋) 获胜！" : "  AI (白棋) 获胜！") << "\n";
        } else {
            std::cout << "  平局！\n";
        }
    }
    std::cout << "════════════════════════════\n\n";
}

Move UI::parseMove(const std::string& input) const {
    std::string s = input;
    // trim
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                     [](unsigned char ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
                          [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());

    int row = -1, col = -1;
    std::istringstream iss(s);
    std::string token;
    std::vector<std::string> tokens;
    while (iss >> token) tokens.push_back(token);

    if (tokens.size() >= 2) {
        auto parseCoord = [](const std::string& t) -> int {
            if (t.length() == 1 && std::isalpha(t[0]))
                return std::toupper(t[0]) - 'A' + 10;
            try { return std::stoi(t); } catch (...) { return -1; }
        };
        row = parseCoord(tokens[0]);
        col = parseCoord(tokens[1]);
    }
    return Move(row, col);
}

void UI::run() {
    printHeader();

    // --- setup ---
    std::string input;
    std::cout << "选择模式:\n"
              << "  1. 人vsAI (你先手)\n"
              << "  2. 人vsAI (AI先手)\n"
              << "  3. AI vs AI\n"
              << "输入 1/2/3 [默认1]: ";
    std::getline(std::cin, input);
    if (input == "2") { game.setAIBlack(true); }
    else if (input == "3") { aiVsAI = true; game.setAIBlack(true); }
    else { game.setAIBlack(false); }

    std::cout << "搜索时间(ms) [默认5000]: ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        try { config.timeMs = std::stoi(input); }
        catch (...) { config.timeMs = 5000; }
    }
    if (config.timeMs < 100) config.timeMs = 100;
    game.engine().setConfig(config);

    if (aiVsAI) {
        std::cout << "AI vs AI 延迟(ms) [默认500]: ";
        std::getline(std::cin, input);
        if (!input.empty()) {
            try { aivsaiDelayMs = std::stoi(input); }
            catch (...) { aivsaiDelayMs = 500; }
        }
        if (aivsaiDelayMs < 0) aivsaiDelayMs = 0;
    }

    game.reset();
    drawBoard(game.getBoard(), Move());

    // --- game loop ---
    while (!game.isOver()) {
        if (aiVsAI) {
            // AI vs AI mode
            Move aiMove = game.makeAIMove();
            drawBoard(game.getBoard(), aiMove);
            if (game.isOver()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(aivsaiDelayMs));
            continue;
        }

        // Human vs AI
        bool isAITurn = (game.isAIBlack() && game.getBoard().side() == BLACK) ||
                        (!game.isAIBlack() && game.getBoard().side() == WHITE);

        if (isAITurn) {
            std::cout << "AI 思考中..." << std::endl;
            Move aiMove = game.makeAIMove();
            drawBoard(game.getBoard(), aiMove);
        } else {
            std::cout << "你的回合 (" << stoneChar(game.getBoard().side()) << "). 输入 行 列 (如: 7 7): ";
            std::getline(std::cin, input);

            if (input == "q" || input == "quit") break;
            if (input == "r" || input == "restart") { game.reset(); drawBoard(game.getBoard(), Move()); continue; }

            Move m = parseMove(input);
            if (!m.valid() || !game.makePlayerMove(m.row, m.col)) {
                std::cout << "无效着法，请重试。\n";
                continue;
            }
            drawBoard(game.getBoard(), Move());
        }
    }

    showResult(game.result());
}
