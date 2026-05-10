#pragma once

#include <cstdint>
#include <cstring>
#include "board.h"

constexpr int TT_BUCKET_SIZE = 2;

enum class Bound { EXACT, LOWER, UPPER, NONE };

struct TTEntry {
    uint64_t key = 0;
    int      depth = -1;
    int      value = 0;
    Bound    bound = Bound::NONE;
    Move     bestMove;
};

struct TTBucket {
    TTEntry entries[TT_BUCKET_SIZE];
};

class TranspositionTable {
public:
    TranspositionTable();
    ~TranspositionTable();

    // mb = desired size in MiB (default 64)
    void resize(int mb);

    void clear();

    TTEntry* probe(uint64_t key);
    const TTEntry* probe(uint64_t key) const;

    void store(uint64_t key, int depth, int value, Bound bound, Move bestMove);

    size_t size() const { return numBuckets; }

private:
    TTBucket* table = nullptr;
    size_t numBuckets = 0;
};
