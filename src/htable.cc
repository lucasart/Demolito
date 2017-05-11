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

HashEntry *HashTable = NULL;
uint64_t HashCount = 0;

int score_to_hash(int score, int ply)
{
    return score >= mate_in(MAX_PLY) ? score + ply
           : score <= mated_in(MAX_PLY) ? score - ply
           : score;
}

int score_from_hash(int hashScore, int ply)
{
    return hashScore >= mate_in(MAX_PLY) ? hashScore - ply
           : hashScore <= mated_in(MAX_PLY) ? hashScore + ply
           : hashScore;
}

void hash_resize(uint64_t hashMB)
{
    const uint64_t newCount = (hashMB << 20) / sizeof(HashEntry);

    HashTable = (HashEntry*)realloc(HashTable, newCount * sizeof(HashEntry));  // FIXME: C++ needs cast

    if (newCount > HashCount)
        memset(&HashTable[HashCount], 0, (newCount - HashCount) * sizeof(HashEntry));

    HashCount = newCount;
}

bool hash_read(uint64_t key, HashEntry *e)
{
    const size_t idx = key & (HashCount - 1);
    *e = HashTable[idx];
    return (e->keyXorData ^ e->data) == key;
}

void hash_write(uint64_t key, const HashEntry *e)
{
    HashEntry *replace = &HashTable[key & (HashCount - 1)];

    if (key != (replace->keyXorData ^ replace->data) || e->depth >= replace->depth)
        *replace = *e;
}
