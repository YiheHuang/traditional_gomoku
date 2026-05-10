#include "game.h"

Game::Game() { reset(); }

void Game::reset() {
    board.reset();
}

bool Game::makePlayerMove(int row, int col) {
    if (!board.isEmpty(row, col)) return false;
    if (!Board::inBounds(row, col)) return false;
    board.makeMove(row, col);
    return true;
}

Move Game::makeAIMove() {
    SearchResult res = ai.search(board);
    if (res.bestMove.valid()) {
        board.makeMove(res.bestMove.row, res.bestMove.col);
    }
    return res.bestMove;
}

void Game::applyMove(Move m) {
    if (m.valid()) board.makeMove(m.row, m.col);
}

GameResult Game::result() const {
    int w = board.checkWinner();
    if (w == BLACK) return GameResult::BLACK_WIN;
    if (w == WHITE) return GameResult::WHITE_WIN;
    if (board.isFull()) return GameResult::DRAW;
    return GameResult::ONGOING;
}

bool Game::isOver() const { return result() != GameResult::ONGOING; }
