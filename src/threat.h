#pragma once

#include "board.h"

// Threat-space search for forced winning sequences (VCT/VCF).
// Returns a winning move if found, or Move(-1,-1) if none.
class ThreatSearch {
public:
    ThreatSearch();

    // Search for a forced win for `attacker` within `maxDepth` plies.
    // Returns the first move of the winning sequence.
    Move searchVCT(const Board& board, int maxDepth = 15);

    // Quick VCF search (only four-threats + five)
    Move searchVCF(const Board& board, int maxDepth = 15);

private:
    // threat types used internally
public:
    enum ThreatType { THR_NONE, THR_FOUR, THR_OPEN_THREE };

    struct Threat {
        Move move;
        ThreatType type;
    };

    bool findThreats(const Board& board, int attacker, std::vector<Threat>& threats);

    bool findDefenses(const Board& board, int attacker, const Move& attackMove,
                      std::vector<Move>& defenses);

    bool vctSearch(Board& board, int attacker, int depth, int maxDepth);
    bool vcfSearch(Board& board, int attacker, int depth, int maxDepth);

    Move winningMove;
};
