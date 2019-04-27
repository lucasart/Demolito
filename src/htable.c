/*
 * Demolito, a UCI chess engine.
 * Copyright 2015 lucasart.
 *
 * Demolito is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Demolito is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If
 * not, see <http://www.gnu.org/licenses/>.
*/
#include <stdlib.h>
#include <string.h>
#include "htable.h"
#include "search.h"

unsigned hashDate = 0;
HashEntry *HashTable = NULL;
static uint64_t HashCount = 0;

static int score_to_hash(int score, int ply)
{
    if (score >= mate_in(MAX_PLY))
        score += ply;
    else if (score <= mated_in(MAX_PLY))
        score -= ply;

    assert(abs(score) < MATE);

    return score;
}

static int score_from_hash(int hashScore, int ply)
{
    if (hashScore >= mate_in(MAX_PLY))
        hashScore -= ply;
    else if (hashScore <= mated_in(MAX_PLY))
        hashScore += ply;

    assert(abs(hashScore) < MATE - ply);

    return hashScore;
}

void *my_aligned_alloc(size_t align, size_t size)
{
    // align must be a power of two, at least the pointer size, to avoid misaligned memory access
    assert(align >= sizeof(uintptr_t) && !(align & (align - 1)) && size);

    // allocate extra head room to store and pointer and padding bytes to align
    const uintptr_t base = (uintptr_t)malloc(size + sizeof(uintptr_t) + align - 1);

    // start adress of the aligned memory chunk (ie. return value)
    const uintptr_t start = (base + sizeof(uintptr_t) + align - 1) & ~(align - 1);

    // Remember the original base pointer returned by malloc(), so that we can free() it.
    *((uintptr_t *)start - 1) = base;

    return (void *)start;
}

void my_aligned_free(void *p)
// WARNING: Only use if p was created by my_aligned_alloc()
{
    if (p)
        // Original base pointer is stored sizeof(uintptr_t) bytes behind p
        free((void *)*((uintptr_t *)p - 1));
}

void hash_prepare(uint64_t hashMB)
{
    my_aligned_free(HashTable);
    HashTable = my_aligned_alloc(sizeof(HashEntry), hashMB << 20);
    HashCount = (hashMB << 20) / sizeof(HashEntry);
    memset(HashTable, 0, hashMB << 20);
}

bool hash_read(uint64_t key, HashEntry *e, int ply)
{
    *e = HashTable[key & (HashCount - 1)];

    if ((e->keyXorData ^ e->data) == key) {
        e->score = score_from_hash(e->score, ply);
        return true;
    }

    e->data = 0;
    return false;
}

void hash_write(uint64_t key, HashEntry *e, int ply)
{
    HashEntry *slot = &HashTable[key & (HashCount - 1)];

    e->date = hashDate;
    assert(e->date == hashDate % 32);

    if (e->date != slot->date || e->depth >= slot->depth) {
        e->score = score_to_hash(e->score, ply);
        e->keyXorData = key ^ e->data;
        *slot = *e;
    }
}

int hash_permille()
{
    int result = 0;

    for (int i = 0; i < 1000; i++)
        result += HashTable[i].keyXorData && HashTable[i].date == hashDate % 32;

    return result;
}
