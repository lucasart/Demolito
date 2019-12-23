#include <assert.h>
#include "util.h"

// Hash function I "invented", or rather derived from SplitMix64. Known limitations:
// - alignment: 'buffer' can be of any 'length', but must be 8-byte aligned.
// - length: must be a multiple of 8 bytes.
// - endianness: naively processes 8-byte blocks without caring for endianness.
// - seedless: you can use a seed argument as initial value for seed variable if needed.
// - not cryptographically secure.
// All the above limitations could easily be adressed, but there is no need for now.
uint64_t hash(const void *buffer, size_t length)
{
    assert((uintptr_t)buffer % 8 == 0 && length % 8 == 0);
    const uint64_t *blocks = (const uint64_t *)buffer;
    uint64_t result = 0, seed = 0;

    for (size_t i = 0; i < length / 8; i++) {
        seed ^= blocks[i];
        result ^= prng(&seed);
    }

    return result;
}

// SplitMix64 PRNG, based on http://xoroshiro.di.unimi.it/splitmix64.c
uint64_t prng(uint64_t *state)
{
    uint64_t rnd = (*state += 0x9E3779B97F4A7C15);
    rnd = (rnd ^ (rnd >> 30)) * 0xBF58476D1CE4E5B9;
    rnd = (rnd ^ (rnd >> 27)) * 0x94D049BB133111EB;
    rnd ^= rnd >> 31;
    assert(rnd);  // We cannot have a zero key for zobrist hashing. If it happens, change the seed.
    return rnd;
}
