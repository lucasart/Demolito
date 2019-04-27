#pragma once
#include <inttypes.h>
#include <stdbool.h>
#include "position.h"

enum {LBOUND, EXACT, UBOUND};

extern unsigned hashDate;

typedef struct {
    uint64_t keyXorData;
    union {
        uint64_t data;
        struct {
            int16_t score, eval;
            move_t move;
            int8_t depth;
            uint8_t bound: 2, singular: 1, date: 5;
        };
    };
} HashEntry;

void *my_aligned_alloc(size_t align, size_t size);
void my_aligned_free(void *p);

void hash_prepare(uint64_t hashMB);  // realloc + clear
bool hash_read(uint64_t key, HashEntry *e, int ply);
void hash_write(uint64_t key, HashEntry *e, int ply);

int hash_permille();

extern HashEntry *HashTable;
