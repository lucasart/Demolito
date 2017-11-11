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
#include "htable.h"

unsigned hash_date;
HashEntry *HashTable = NULL;
static uint64_t HashCount = 0;

int score_to_hash(int score, int ply)
{
    assert(abs(score) < MATE);
    return score >= mate_in(MAX_PLY) ? score + ply
        : score <= mated_in(MAX_PLY) ? score - ply
        : score;
}

int score_from_hash(int hashScore, int ply)
{
    assert(abs(hashScore) < MATE);
    return hashScore >= mate_in(MAX_PLY) ? hashScore - ply
        : hashScore <= mated_in(MAX_PLY) ? hashScore + ply
        : hashScore;
}

void hash_resize(uint64_t hashMB)
{
    free(HashTable);
#ifdef _WIN64  // C11 support lacking
    HashTable = malloc(hashMB << 20);
#else
    HashTable = aligned_alloc(sizeof(HashEntry), hashMB << 20);
#endif
    HashCount = (hashMB << 20) / sizeof(HashEntry);
}

bool hash_read(uint64_t key, HashEntry *e)
{
    *e = HashTable[key & (HashCount - 1)];
    return (e->keyXorData ^ e->data) == key;
}

void hash_write(uint64_t key, const HashEntry *e)
{
    HashEntry *slot = &HashTable[key & (HashCount - 1)];

    if (e->date != slot->date || e->depth >= slot->depth)
        *slot = *e;
}
