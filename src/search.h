#pragma once

#include <chrono>
#include <atomic>
#include <vector>
#include <utility>
#include "board.h"
#include "movegen.h"
#include "evaluator.h"
#include "transposition.h"
#include "threat.h"

struct SearchConfig {
    int maxDepth = 64;     // hard depth cap
    int timeMs = 5000;     // time budget per move (ms)
    bool useTT = true;
    bool useKiller = true;
    bool useHistory = true;
    bool useVCT = true;
    int vctDepth = 15;
};

struct SearchResult {
    Move bestMove;
    int  score = 0;
    int  depth = 0;
    int  nodes = 0;
    int  tbHits = 0;
    int  tbMisses = 0;
    double timeMs = 0;
};

class SearchEngine {
public:
    SearchEngine();

    void setConfig(const SearchConfig& cfg) { config = cfg; }
    SearchConfig& getConfig() { return config; }

    SearchResult search(const Board& board);

    void stop();
    void clear();

    Evaluator& evaluator() { return eval; }
    MoveGenerator& moveGen() { return movegen; }
    TranspositionTable& transTable() { return tt; }

    // After search(), returns (move, score) for all root candidates.
    // Scores are from the current player's perspective (same convention as alphaBeta).
    const std::vector<std::pair<Move, int>>& getRootScores() const { return rootScores; }

private:
    SearchConfig config;
    Evaluator eval;
    MoveGenerator movegen;
    TranspositionTable tt;
    ThreatSearch threat;

    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> stopped;
    int totalNodes;
    int ttHits, ttMisses;

    std::vector<std::pair<Move, int>> rootScores;

    bool outOfTime() const;

    int alphaBeta(Board& board, int depth, int alpha, int beta, int ply, bool isNullMove);

    int quiescence(Board& board, int alpha, int beta, int ply);

    Move getBestMoveFromTT(const Board& board) const;
};
