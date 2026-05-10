#include "movegen.h"
#include "pattern.h"
#include <algorithm>
#include <cstring>

static constexpr int MAX_CANDIDATES = 18;

MoveGenerator::MoveGenerator() {
    clearKillers();
    historyTable.fill(0);
}

void MoveGenerator::clearKillers() {
    for (auto& k : killers) { k[0] = Move(); k[1] = Move(); }
}

void MoveGenerator::addKiller(int depth, Move m) {
    if (depth < 0 || depth >= static_cast<int>(killers.size())) return;
    if (killers[depth][0] != m) {
        killers[depth][1] = killers[depth][0];
        killers[depth][0] = m;
    }
}

void MoveGenerator::addHistory(Move m, int depth) {
    historyTable[m.index()] += depth * depth;
}

void MoveGenerator::ageHistory() {
    for (auto& v : historyTable) v /= 2;
}

int MoveGenerator::scoreMove(const Move& m, const Board& /*board*/, int depth) const {
    if (depth >= 0 && depth < static_cast<int>(killers.size())) {
        if (m == killers[depth][0]) return 900000;
        if (m == killers[depth][1]) return 800000;
    }
    return std::min(historyTable[m.index()], 700000);
}

void MoveGenerator::sortMoves(std::vector<Move>& moves, const Board& board, int depth) {
    const int* b = board.raw();
    int side = board.side();
    int opp = board.opp();

    std::vector<std::pair<int, int>> scored;
    scored.reserve(moves.size());
    for (size_t i = 0; i < moves.size(); ++i) {
        const Move& m = moves[i];
        int att = quickScore(b, m.row, m.col, side);
        int def = quickScore(b, m.row, m.col, opp);
        int s = att + def / 10 + scoreMove(m, board, depth);
        scored.push_back({s, static_cast<int>(i)});
    }

    std::sort(scored.begin(), scored.end(),
              [](auto& a, auto& b) { return a.first > b.first; });

    std::vector<Move> sorted;
    sorted.reserve(moves.size());
    for (auto& p : scored) sorted.push_back(moves[p.second]);
    moves = std::move(sorted);
}

std::vector<Move> MoveGenerator::generateMoves(const Board& board) {
    if (board.ply() == 0) return {Move(7, 7)};

    bool visited[BOARD_CELLS] = {false};
    std::vector<Move> moves;

    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (board.isEmpty(r, c)) continue;
            for (int dr = -2; dr <= 2; ++dr)
                for (int dc = -2; dc <= 2; ++dc) {
                    int nr = r + dr, nc = c + dc;
                    if (!Board::inBounds(nr, nc)) continue;
                    if (!board.isEmpty(nr, nc)) continue;
                    int idx = nr * BOARD_SIZE + nc;
                    if (visited[idx]) continue;
                    visited[idx] = true;
                    moves.push_back(Move(nr, nc));
                }
        }

    // Limit to top N candidates
    if (static_cast<int>(moves.size()) > MAX_CANDIDATES) {
        const int* b = board.raw();
        int side = board.side();
        int opp = board.opp();

        std::vector<std::pair<int, Move>> scored;
        scored.reserve(moves.size());
        for (const auto& m : moves) {
            int att = quickScore(b, m.row, m.col, side);
            int def = quickScore(b, m.row, m.col, opp);
            scored.push_back({att + def / 10, m});
        }
        std::nth_element(scored.begin(), scored.begin() + MAX_CANDIDATES, scored.end(),
                         [](const auto& a, const auto& b) { return a.first > b.first; });
        scored.resize(MAX_CANDIDATES);

        moves.clear();
        for (auto& p : scored) moves.push_back(p.second);
    }

    return moves;
}
