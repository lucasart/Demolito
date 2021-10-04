/*
 * Demolito, a UCI chess engine. Copyright 2015-2020 lucasart.
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
#include "util.h"
#include <assert.h>

static uint64_t hash_mix(uint64_t block) {
    block ^= block >> 23;
    block *= 0x2127599bf4325c37ULL;
    return block ^= block >> 47;
}

// SplitMix64 PRNG, based on http://xoroshiro.di.unimi.it/splitmix64.c
uint64_t prng(uint64_t *state) {
    uint64_t rnd = (*state += 0x9E3779B97F4A7C15);
    rnd = (rnd ^ (rnd >> 30)) * 0xBF58476D1CE4E5B9;
    rnd = (rnd ^ (rnd >> 27)) * 0x94D049BB133111EB;
    rnd ^= rnd >> 31;
    return rnd;
}

double prngf(uint64_t *state) {
    return (prng(state) >> 11) * 0x1.0p-53;
}

void hash_block(uint64_t block, uint64_t *hash) {
    *hash ^= hash_mix(block);
    *hash *= 0x880355f21e6d1965ULL;
}

// Based on FastHash64, without length hashing, to make it capable of incremental updates. Assumes
// length is divisible by 8, and buffer is 8-byte aligned.
void hash_blocks(const void *buffer, size_t length, uint64_t *hash) {
    assert((uintptr_t)buffer % 8 == 0 && length % 8 == 0);
    const uint64_t *blocks = (const uint64_t *)buffer;

    for (size_t i = 0; i < length / 8; i++)
        hash_block(*blocks++, hash);
}
