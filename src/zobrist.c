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
#include "zobrist.h"
#include "bitboard.h"

uint64_t ZobristKey[NB_COLOR][NB_PIECE][NB_SQUARE];
static uint64_t ZobristCastling[NB_SQUARE];
uint64_t ZobristEnPassant[NB_SQUARE + 1];
uint64_t ZobristTurn;

void stack_clear(Stack *gs)
{
    gs->idx = 0;
}

void stack_push(Stack *gs, uint64_t key)
{
    assert(0 <= gs->idx && gs->idx < MAX_GAME_PLY);
    gs->keys[gs->idx++] = key;
}

void stack_pop(Stack *gs)
{
    assert(0 < gs->idx && gs->idx <= MAX_GAME_PLY);
    gs->idx--;
}

uint64_t stack_back(const Stack *gs)
{
    assert(0 < gs->idx && gs->idx <= MAX_GAME_PLY);
    return gs->keys[gs->idx - 1];
}

uint64_t stack_move_key(const Stack *gs)
{
    assert(0 < gs->idx && gs->idx <= MAX_GAME_PLY);
    return gs->idx > 1 ? gs->keys[gs->idx - 1] ^ gs->keys[gs->idx - 2] : 0;
}

bool stack_repetition(const Stack *gs, int rule50)
{
    // 50 move rule
    if (rule50 >= 100)
        return true;

    // TODO: use 3 repetition past root position
    for (int i = 4; i <= rule50 && i < gs->idx; i += 2)
        if (gs->keys[gs->idx - 1 - i] == gs->keys[gs->idx - 1])
            return true;

    return false;
}

// SplitMix64 PRNG, by Sebastiano Vigna: http://xoroshiro.di.unimi.it/splitmix64.c
// 64-bit state is enough:
//   * 2^64 period is long enough to generate a few zobrist keys.
//   * Statistically strong: passes TestU01's BigCrunch.
// 64-bit state is, in fact, a huge bonus:
//   * Easy to seed: Any seed is fine, even zero!
//   * Escapes immediately from zero land, unlike large seed generators (MT being, by far, the
//     worst, taking up to hundreds of thousands of drawings to fully escape zero land; which is
//     why proper seeding is crucial with large generators, that need another generator just to
//     seed themselves).
// Fast enough: 1.31ns per drawing on i7 7700 CPU @ 3.6 GHz (Kaby Lake).
uint64_t prng(uint64_t *state)
{
    uint64_t s = (*state += 0x9E3779B97F4A7C15ULL);
    s = (s ^ (s >> 30)) * 0xBF58476D1CE4E5B9ULL;
    s = (s ^ (s >> 27)) * 0x94D049BB133111EBULL;
    return s ^ (s >> 31);
}

void zobrist_init()
{
    uint64_t state = 0;

    for (int c = WHITE; c <= BLACK; ++c)
        for (int p = KNIGHT; p < NB_PIECE; ++p)
            for (int s = A1; s <= H8; ++s)
                ZobristKey[c][p][s] = prng(&state);

    for (int s = A1; s <= H8; ++s)
        ZobristCastling[s] = prng(&state);

    for (int s = A1; s <= H8; ++s)
        ZobristEnPassant[s] = prng(&state);

    ZobristEnPassant[NB_SQUARE] = prng(&state);
    ZobristTurn = prng(&state);
}

uint64_t zobrist_keys(int c, int p, uint64_t sqs)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p, NB_PIECE);

    bitboard_t k = 0;

    while(sqs)
        k ^= ZobristKey[c][p][bb_pop_lsb(&sqs)];

    return k;
}

uint64_t zobrist_castling(bitboard_t castlableRooks)
{
    bitboard_t k = 0;

    while (castlableRooks)
        k ^= ZobristCastling[bb_pop_lsb(&castlableRooks)];

    return k;
}
