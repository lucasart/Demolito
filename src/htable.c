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

void hash_prepare(uint64_t hashMB)
{
    free(HashTable);
#ifdef _WIN64  // C11 support lacking, use instead Microsoft's _aligned_malloc()
    HashTable = _aligned_malloc(hashMB << 20, sizeof(HashEntry));
#elif defined(__APPLE__)
    HashTable = calloc(hashMB << 20, sizeof(HashEntry));
#else
    HashTable = aligned_alloc(sizeof(HashEntry), hashMB << 20);
#endif
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
