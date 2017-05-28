#pragma once
#include "move.h"

enum {LBOUND, EXACT, UBOUND};

typedef struct {
    uint64_t keyXorData;
    union {
        uint64_t data;
        struct {
            int16_t score, eval, move;
            int8_t depth, bound;
        };
    };
} HashEntry;

// Adjust mate scores to plies from current position, instead of plies from root
int score_to_hash(int score, int ply);
int score_from_hash(int hashScore, int ply);

void hash_resize(uint64_t hashMB);
bool hash_read(uint64_t key, HashEntry *e);
void hash_write(uint64_t key, const HashEntry *e);

extern HashEntry *HashTable;
extern size_t HashCount;
