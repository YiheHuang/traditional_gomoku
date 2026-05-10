#include "search.h"
#include "pattern.h"
#include <algorithm>
#include <iostream>

SearchEngine::SearchEngine() {
    stopped.store(false);
}

void SearchEngine::stop() { stopped.store(true); }
void SearchEngine::clear() { tt.clear(); movegen.clearKillers(); }

bool SearchEngine::outOfTime() const {
    if (stopped.load()) return true;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    return elapsed >= config.timeMs;
}

Move SearchEngine::getBestMoveFromTT(const Board& board) const {
    const TTEntry* entry = tt.probe(board.hash());
    if (entry && entry->bestMove.valid()) return entry->bestMove;
    return Move();
}

// ---------------------------------------------------------------------------
// Quiescence search — only consider immediate five / open-four threats.
// Limited to Q_MAX_DEPTH plies of recursion.
// ---------------------------------------------------------------------------
static constexpr int Q_MAX_DEPTH = 4;

int SearchEngine::quiescence(Board& board, int alpha, int beta, int ply) {
    ++totalNodes;
    if (outOfTime()) return 0;

    int side = board.side();
    int opp  = board.opp();

    int standPat = eval.evaluate(board, side);
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    // Check if opponent already won
    if (standPat <= -20000000) return standPat;

    // Limit quiescence depth to avoid explosion
    if (ply >= Q_MAX_DEPTH) return standPat;

    const int* b = board.raw();
    std::vector<Move> threats;

    // Build near-set once
    bool near[BOARD_CELLS] = {false};
    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c)
            if (board.get(r, c) != EMPTY)
                for (int dr = -2; dr <= 2; ++dr)
                    for (int dc = -2; dc <= 2; ++dc) {
                        int nr = r + dr, nc = c + dc;
                        if (Board::inBounds(nr, nc) && board.isEmpty(nr, nc))
                            near[nr * BOARD_SIZE + nc] = true;
                    }

    // Prioritize immediates: five + open-four (must-defend-or-die)
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (!near[r * BOARD_SIZE + c]) continue;
            int att = quickScore(b, r, c, side);
            if (att >= VAL_FIVE) {
                // Immediate win — no need to search further
                return 20000000 - ply;
            }
            if (att >= VAL_OPEN_FOUR) {
                threats.push_back(Move(r, c));
            }
        }
    }

    // If no open-four threats, also check fours and strong defense
    if (threats.empty()) {
        for (int r = 0; r < BOARD_SIZE; ++r) {
            for (int c = 0; c < BOARD_SIZE; ++c) {
                if (!near[r * BOARD_SIZE + c]) continue;
                int att = quickScore(b, r, c, side);
                int def = quickScore(b, r, c, opp);
                if (att >= VAL_FOUR || def >= VAL_FOUR) {
                    threats.push_back(Move(r, c));
                }
            }
        }
    }

    // Limit to top N threats
    constexpr int Q_MAX_MOVES = 12;
    if (static_cast<int>(threats.size()) > Q_MAX_MOVES) {
        std::nth_element(threats.begin(), threats.begin() + Q_MAX_MOVES, threats.end(),
                         [&](const Move& ma, const Move& mb) {
                             int sa = std::max(quickScore(b, ma.row, ma.col, side),
                                               quickScore(b, ma.row, ma.col, opp));
                             int sb = std::max(quickScore(b, mb.row, mb.col, side),
                                               quickScore(b, mb.row, mb.col, opp));
                             return sa > sb;
                         });
        threats.resize(Q_MAX_MOVES);
    }

    // Sort by threat score
    std::sort(threats.begin(), threats.end(), [&](const Move& ma, const Move& mb) {
        int sa = std::max(quickScore(b, ma.row, ma.col, side),
                          quickScore(b, ma.row, ma.col, opp));
        int sb = std::max(quickScore(b, mb.row, mb.col, side),
                          quickScore(b, mb.row, mb.col, opp));
        return sa > sb;
    });

    for (const auto& mv : threats) {
        board.makeMove(mv.row, mv.col);
        int score = -quiescence(board, -beta, -alpha, ply + 1);
        board.undoMove();

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

// ---------------------------------------------------------------------------
// Alpha-beta with PVS.
// ---------------------------------------------------------------------------
int SearchEngine::alphaBeta(Board& board, int depth, int alpha, int beta,
                             int ply, bool /*isNullMove*/) {
    ++totalNodes;

    if (depth <= 0)
        return quiescence(board, alpha, beta, 0);

    if (outOfTime()) return 0;

    int winner = board.checkWinner();
    if (winner == board.side()) return 20000000 - ply;
    if (winner == board.opp())  return -20000000 + ply;
    if (board.isFull()) return 0;

    // --- transposition table probe ---
    if (config.useTT) {
        const TTEntry* entry = tt.probe(board.hash());
        if (entry && entry->depth >= depth) {
            ++ttHits;
            if (entry->bound == Bound::EXACT) return entry->value;
            if (entry->bound == Bound::LOWER && entry->value >= beta) return entry->value;
            if (entry->bound == Bound::UPPER && entry->value <= alpha) return entry->value;
        } else {
            ++ttMisses;
        }
    }

    // --- move generation ---
    std::vector<Move> moves = movegen.generateMoves(board);
    if (moves.empty()) return 0;

    // Insert TT best-move at front
    if (config.useTT) {
        Move ttMove = getBestMoveFromTT(board);
        if (ttMove.valid()) {
            for (auto it = moves.begin(); it != moves.end(); ++it) {
                if (*it == ttMove) { std::swap(*moves.begin(), *it); break; }
            }
        }
    }

    movegen.sortMoves(moves, board, ply);

    // --- PVS loop ---
    int bestVal = -999999999;
    Move bestMove = moves.empty() ? Move() : moves[0];
    Bound bound = Bound::UPPER;

    for (size_t i = 0; i < moves.size(); ++i) {
        board.makeMove(moves[i].row, moves[i].col);

        int score;
        if (i == 0) {
            score = -alphaBeta(board, depth - 1, -beta, -alpha, ply + 1, false);
        } else {
            // Zero-width window search first
            score = -alphaBeta(board, depth - 1, -alpha - 1, -alpha, ply + 1, false);
            if (score > alpha && score < beta && !outOfTime()) {
                score = -alphaBeta(board, depth - 1, -beta, -alpha, ply + 1, false);
            }
        }

        board.undoMove();

        if (score > bestVal) {
            bestVal = score;
            bestMove = moves[i];
            if (score > alpha) {
                alpha = score;
                bound = Bound::EXACT;
            }
        }

        if (alpha >= beta) {
            bound = Bound::LOWER;
            if (config.useKiller) movegen.addKiller(ply, moves[i]);
            if (config.useHistory) movegen.addHistory(moves[i], depth);
            break; // beta cutoff
        }
    }

    // --- store in TT ---
    if (config.useTT && !outOfTime()) {
        tt.store(board.hash(), depth, bestVal, bound, bestMove);
    }

    return bestVal;
}

// ---------------------------------------------------------------------------
// Main search entry: iterative deepening.
// ---------------------------------------------------------------------------
SearchResult SearchEngine::search(const Board& board) {
    SearchResult result;
    startTime = std::chrono::steady_clock::now();
    stopped.store(false);
    totalNodes = 0;
    ttHits = 0;
    ttMisses = 0;

    Board b = board; // mutable copy

    // --- VCT/VCF first: if a forced win exists, play it immediately ---
    if (config.useVCT) {
        Move vcf = threat.searchVCF(b, config.vctDepth);
        if (vcf.valid()) {
            result.bestMove = vcf;
            result.score = 20000000;
            result.depth = 0;
            auto end = std::chrono::steady_clock::now();
            result.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();
            result.nodes = totalNodes;
            return result;
        }
        Move vct = threat.searchVCT(b, config.vctDepth);
        if (vct.valid()) {
            result.bestMove = vct;
            result.score = 15000000;
            result.depth = 0;
            auto end = std::chrono::steady_clock::now();
            result.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();
            result.nodes = totalNodes;
            return result;
        }
    }

    // --- Iterative deepening ---
    Move bestMoveSoFar;
    int bestScoreSoFar = 0;
    int completedDepth = 0;

    for (int d = 1; d <= config.maxDepth; ++d) {
        alphaBeta(b, d, -999999999, 999999999, 0, false);

        if (outOfTime()) break;

        completedDepth = d;
        // Retrieve best move and score from TT
        const TTEntry* entry = tt.probe(b.hash());
        if (entry && entry->bestMove.valid()) {
            bestMoveSoFar = entry->bestMove;
            bestScoreSoFar = entry->value;
        }
    }

    if (!bestMoveSoFar.valid()) {
        auto moves = movegen.generateMoves(b);
        if (!moves.empty()) {
            movegen.sortMoves(moves, b, 0);
            bestMoveSoFar = moves[0];
        }
    }

    auto end = std::chrono::steady_clock::now();

    result.bestMove = bestMoveSoFar;
    result.score    = bestScoreSoFar;
    result.depth    = completedDepth;
    result.nodes    = totalNodes;
    result.tbHits   = ttHits;
    result.tbMisses = ttMisses;
    result.timeMs   = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();

    std::cout << "[搜索] 深度:" << completedDepth
              << " 评分:" << bestScoreSoFar
              << " 节点:" << totalNodes
              << " TT命中:" << ttHits
              << " TT未命中:" << ttMisses
              << " 用时:" << result.timeMs << "ms" << std::endl;

    return result;
}
