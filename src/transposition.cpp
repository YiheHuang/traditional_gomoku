#include "transposition.h"
#include <cstdlib>

TranspositionTable::TranspositionTable() { resize(64); }

TranspositionTable::~TranspositionTable() { std::free(table); }

void TranspositionTable::resize(int mb) {
    std::free(table);
    table = nullptr;
    size_t bytes = static_cast<size_t>(mb) * 1024 * 1024;
    numBuckets = bytes / sizeof(TTBucket);
    if (numBuckets < 256) numBuckets = 256;
    table = static_cast<TTBucket*>(std::calloc(numBuckets, sizeof(TTBucket)));
}

void TranspositionTable::clear() {
    if (table) { for (size_t i = 0; i < numBuckets; ++i) table[i] = TTBucket(); }
}

TTEntry* TranspositionTable::probe(uint64_t key) {
    if (!table) return nullptr;
    size_t idx = key % numBuckets;
    for (int i = 0; i < TT_BUCKET_SIZE; ++i) {
        if (table[idx].entries[i].key == key)
            return &table[idx].entries[i];
    }
    return nullptr;
}

const TTEntry* TranspositionTable::probe(uint64_t key) const {
    if (!table) return nullptr;
    size_t idx = key % numBuckets;
    for (int i = 0; i < TT_BUCKET_SIZE; ++i) {
        if (table[idx].entries[i].key == key)
            return &table[idx].entries[i];
    }
    return nullptr;
}

void TranspositionTable::store(uint64_t key, int depth, int value, Bound bound, Move bestMove) {
    if (!table) return;
    size_t idx = key % numBuckets;
    int replaceIdx = 0;
    int worstDepth = table[idx].entries[0].depth;

    for (int i = 0; i < TT_BUCKET_SIZE; ++i) {
        if (table[idx].entries[i].key == key) {
            // Same position: only overwrite if stored depth is shallower
            if (depth >= table[idx].entries[i].depth) {
                replaceIdx = i;
                break;
            }
            return; // keep deeper entry
        }
        if (table[idx].entries[i].depth < worstDepth) {
            worstDepth = table[idx].entries[i].depth;
            replaceIdx = i;
        }
    }

    table[idx].entries[replaceIdx].key   = key;
    table[idx].entries[replaceIdx].depth = depth;
    table[idx].entries[replaceIdx].value = value;
    table[idx].entries[replaceIdx].bound = bound;
    table[idx].entries[replaceIdx].bestMove = bestMove;
}
