#include "threat.h"
#include "pattern.h"
#include <vector>
#include <algorithm>

ThreatSearch::ThreatSearch() {}

// ---------------------------------------------------------------------------
// Check whether placing (r,c) for `attacker` creates a four or open-three.
// ---------------------------------------------------------------------------
static ThreatSearch::ThreatType classifyThreat(const int* b, int r, int c,
                                                int attacker) {
    DirResult dr[DIR_NUM];
    for (int d = 0; d < DIR_NUM; ++d)
        dr[d] = analyseDir(b, r, c, DIR_DR[d], DIR_DC[d], attacker);

    bool hasFour       = false;
    bool hasOpenThree  = false;

    for (int d = 0; d < DIR_NUM; ++d) {
        if (dr[d].count >= 5) return ThreatSearch::THR_FOUR; // immediate five
        if (dr[d].count == 4 && dr[d].openEnds >= 1) hasFour = true;
        if (dr[d].count == 3 && dr[d].openEnds == 2) hasOpenThree = true;
    }

    // Also check compound threats
    int fours = 0, openThrees = 0;
    for (int d = 0; d < DIR_NUM; ++d) {
        if (dr[d].count == 4 && dr[d].openEnds >= 1) ++fours;
        if (dr[d].count == 3 && dr[d].openEnds == 2) ++openThrees;
    }
    if (fours >= 2 || (fours >= 1 && openThrees >= 1)) return ThreatSearch::THR_FOUR;
    if (openThrees >= 2) return ThreatSearch::THR_FOUR; // double open-three

    if (hasFour)       return ThreatSearch::THR_FOUR;
    if (hasOpenThree)  return ThreatSearch::THR_OPEN_THREE;
    return ThreatSearch::THR_NONE;
}

// ---------------------------------------------------------------------------
// Find all threatening moves for `attacker`.
// ---------------------------------------------------------------------------
bool ThreatSearch::findThreats(const Board& board, int attacker,
                               std::vector<Threat>& threats) {
    threats.clear();
    const int* b = board.raw();

    // Only check cells near existing stones
    bool near[BOARD_CELLS] = {false};
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (board.get(r, c) == EMPTY) continue;
            for (int dr = -2; dr <= 2; ++dr) {
                for (int dc = -2; dc <= 2; ++dc) {
                    int nr = r + dr, nc = c + dc;
                    if (Board::inBounds(nr, nc) && board.isEmpty(nr, nc))
                        near[nr * BOARD_SIZE + nc] = true;
                }
            }
        }
    }

    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (!near[r * BOARD_SIZE + c]) continue;
            // Also check own attacks — a five ends the game
            int quickAtt = quickScore(b, r, c, attacker);
            if (quickAtt >= VAL_FIVE) {
                threats.push_back({Move(r, c), THR_FOUR});
                return true;
            }
        }
    }

    // Collect threat cells
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (!near[r * BOARD_SIZE + c]) continue;
            ThreatType t = classifyThreat(b, r, c, attacker);
            if (t != THR_NONE) threats.push_back({Move(r, c), t});
        }
    }

    // Sort: four-threats first, then open-threes
    std::sort(threats.begin(), threats.end(), [](const Threat& a, const Threat& b) {
        return static_cast<int>(a.type) > static_cast<int>(b.type);
    });
    return !threats.empty();
}

// ---------------------------------------------------------------------------
// Find all defensive moves against `attackMove`.
// For a four, there is exactly 1 blocking square in each direction.
// For an open-three, there are multiple blocking squares.
// ---------------------------------------------------------------------------
bool ThreatSearch::findDefenses(const Board& board, int attacker,
                                const Move& attackMove,
                                std::vector<Move>& defenses) {
    defenses.clear();
    const int* b = board.raw();
    // The attack cell itself is always a defense (capture it before opponent plays there)
    defenses.push_back(attackMove);

    // For each direction, find blocking squares
    for (int d = 0; d < DIR_NUM; ++d) {
        int dr = DIR_DR[d], dc = DIR_DC[d];

        // Count consecutive attacker stones through attackMove
        int cnt = 1;
        // +dir
        int r1 = attackMove.row + dr, c1 = attackMove.col + dc;
        while (Board::inBounds(r1, c1) && b[r1 * BOARD_SIZE + c1] == attacker) {
            ++cnt; r1 += dr; c1 += dc;
        }
        // -dir
        r1 = attackMove.row - dr; c1 = attackMove.col - dc;
        while (Board::inBounds(r1, c1) && b[r1 * BOARD_SIZE + c1] == attacker) {
            ++cnt; r1 -= dr; c1 -= dc;
        }

        if (cnt >= 5) continue; // already winning

        if (cnt == 4) {
            // Block endpoints
            // +dir end
            r1 = attackMove.row + dr; c1 = attackMove.col + dc;
            while (Board::inBounds(r1, c1) && b[r1 * BOARD_SIZE + c1] == attacker) {
                r1 += dr; c1 += dc;
            }
            if (Board::inBounds(r1, c1) && b[r1 * BOARD_SIZE + c1] == EMPTY)
                defenses.push_back(Move(r1, c1));
            // -dir end
            r1 = attackMove.row - dr; c1 = attackMove.col - dc;
            while (Board::inBounds(r1, c1) && b[r1 * BOARD_SIZE + c1] == attacker) {
                r1 -= dr; c1 -= dc;
            }
            if (Board::inBounds(r1, c1) && b[r1 * BOARD_SIZE + c1] == EMPTY)
                defenses.push_back(Move(r1, c1));
        } else if (cnt == 3) {
            // Block both open ends if open-three
            // +dir end
            r1 = attackMove.row + dr; c1 = attackMove.col + dc;
            while (Board::inBounds(r1, c1) && b[r1 * BOARD_SIZE + c1] == attacker) {
                r1 += dr; c1 += dc;
            }
            if (Board::inBounds(r1, c1) && b[r1 * BOARD_SIZE + c1] == EMPTY)
                defenses.push_back(Move(r1, c1));
            // -dir end
            r1 = attackMove.row - dr; c1 = attackMove.col - dc;
            while (Board::inBounds(r1, c1) && b[r1 * BOARD_SIZE + c1] == attacker) {
                r1 -= dr; c1 -= dc;
            }
            if (Board::inBounds(r1, c1) && b[r1 * BOARD_SIZE + c1] == EMPTY)
                defenses.push_back(Move(r1, c1));
            // The middle gap (for jump patterns) - simplified
            // +dir one step past
            r1 = attackMove.row + dr; c1 = attackMove.col + dc;
            if (Board::inBounds(r1, c1) && b[r1 * BOARD_SIZE + c1] == EMPTY) {
                int r2 = r1 + dr, c2 = c1 + dc;
                if (Board::inBounds(r2, c2) && b[r2 * BOARD_SIZE + c2] == attacker)
                    defenses.push_back(Move(r1, c1));
            }
        }
    }

    // Remove duplicates and invalid moves
    std::sort(defenses.begin(), defenses.end(),
              [](const Move& a, const Move& b) {
                  return a.index() < b.index();
              });
    defenses.erase(std::unique(defenses.begin(), defenses.end(),
                               [](const Move& a, const Move& b) {
                                   return a.index() == b.index();
                               }),
                   defenses.end());

    return !defenses.empty();
}

// ---------------------------------------------------------------------------
// VCF search: only four-threat sequences (faster, more common).
// ---------------------------------------------------------------------------
bool ThreatSearch::vcfSearch(Board& board, int attacker, int depth, int maxDepth) {
    if (depth > maxDepth) return false;

    int w = board.checkWinner();
    if (w == attacker) { winningMove = board.lastMove(); return true; }
    if (w != EMPTY) return false;

    // Attacker: find all four-threats
    std::vector<Threat> threats;
    findThreats(board, attacker, threats);

    Move savedWin = winningMove; // save in case this branch fails

    for (const auto& th : threats) {
        if (th.type != THR_FOUR) continue; // VCF only uses four-threats

        // Try this attack
        board.makeMove(th.move.row, th.move.col);
        int subWinner = board.checkWinner();
        if (subWinner == attacker) {
            winningMove = th.move;
            board.undoMove();
            return true;
        }

        // Defender must block
        std::vector<Move> defenses;
        findDefenses(board, attacker, th.move, defenses);

        bool allDefenseFail = true;
        for (const auto& def : defenses) {
            if (board.get(def.row, def.col) != EMPTY) continue;
            board.makeMove(def.row, def.col);
            bool attackerWins = vcfSearch(board, attacker, depth + 2, maxDepth);
            board.undoMove();
            if (!attackerWins) { allDefenseFail = false; break; }
        }

        board.undoMove();
        if (allDefenseFail && !defenses.empty()) {
            winningMove = th.move;
            return true;
        }
        // Branch failed — restore winningMove to avoid false positives
        winningMove = savedWin;
    }
    return false;
}

// ---------------------------------------------------------------------------
// VCT search: four-threat + open-three threat sequences.
// ---------------------------------------------------------------------------
bool ThreatSearch::vctSearch(Board& board, int attacker, int depth, int maxDepth) {
    if (depth > maxDepth) return false;

    int w = board.checkWinner();
    if (w == attacker) { winningMove = board.lastMove(); return true; }
    if (w != EMPTY) return false;

    // Attacker: find all threats
    std::vector<Threat> threats;
    findThreats(board, attacker, threats);

    Move savedWin = winningMove; // save in case this branch fails

    for (const auto& th : threats) {
        board.makeMove(th.move.row, th.move.col);
        int subWinner = board.checkWinner();
        if (subWinner == attacker) {
            winningMove = th.move;
            board.undoMove();
            return true;
        }

        // Defender must block
        std::vector<Move> defenses;
        findDefenses(board, attacker, th.move, defenses);

        bool allDefenseFail = true;
        for (const auto& def : defenses) {
            if (board.get(def.row, def.col) != EMPTY) continue;
            board.makeMove(def.row, def.col);

            bool attackerWins;
            if (th.type == THR_FOUR)
                attackerWins = vcfSearch(board, attacker, depth + 2, maxDepth);
            else
                attackerWins = vctSearch(board, attacker, depth + 2, maxDepth);

            board.undoMove();
            if (!attackerWins) { allDefenseFail = false; break; }
        }

        board.undoMove();
        if (allDefenseFail && !defenses.empty()) {
            winningMove = th.move;
            return true;
        }
        // Branch failed — restore winningMove to avoid false positives
        winningMove = savedWin;
    }
    return false;
}

// --- public interface ---

Move ThreatSearch::searchVCT(const Board& board, int maxDepth) {
    winningMove = Move();
    Board b = board; // copy
    vctSearch(b, b.side(), 0, maxDepth);
    return winningMove;
}

Move ThreatSearch::searchVCF(const Board& board, int maxDepth) {
    winningMove = Move();
    Board b = board;
    vcfSearch(b, b.side(), 0, maxDepth);
    return winningMove;
}
