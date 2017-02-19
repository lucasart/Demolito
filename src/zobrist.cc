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

namespace {

uint64_t Zobrist[NB_COLOR][NB_PIECE][NB_SQUARE];
uint64_t ZobristCastling[NB_SQUARE];
uint64_t ZobristEnPassant[(int)NB_SQUARE+1];
uint64_t ZobristTurn;

uint64_t rotate(uint64_t x, int k)
{
    return (x << k) | (x >> (64 - k));
}

}    // namespace

namespace zobrist {

void gs_clear(GameStack *gs)
{
    gs->idx = 0;
}

void gs_push(GameStack *gs, uint64_t key)
{
    assert(0 <= gs->idx && gs->idx < MAX_GAME_PLY);
    gs->keys[gs->idx++] = key;
}

void gs_pop(GameStack *gs)
{
    assert(0 < gs->idx && gs->idx <= MAX_GAME_PLY);
    gs->idx--;
}

uint64_t gs_back(const GameStack *gs)
{
    assert(0 < gs->idx && gs->idx <= MAX_GAME_PLY);
    return gs->keys[gs->idx - 1];
}

bool gs_repetition(const GameStack *gs, int rule50)
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

void prng_init(PRNG *prng, uint64_t seed)
{
    prng->a = 0xf1ea5eed;
    prng->b = prng->c = prng->d = seed;

    for (int i = 0; i < 20; ++i)
        prng_rand(prng);
}

uint64_t prng_rand(PRNG *prng)
{
    uint64_t e = prng->a - rotate(prng->b, 7);
    prng->a = prng->b ^ rotate(prng->c, 13);
    prng->b = prng->c + rotate(prng->d, 37);
    prng->c = prng->d + e;
    return prng->d = e + prng->a;
}

void init()
{
    PRNG prng;
    prng_init(&prng, 0);

    for (int c = WHITE; c <= BLACK; ++c)
        for (int p = KNIGHT; p < NB_PIECE; ++p)
            for (int s = A1; s <= H8; ++s)
                Zobrist[c][p][s] = prng_rand(&prng);

    for (int s = A1; s <= H8; ++s)
        ZobristCastling[s] = prng_rand(&prng);

    for (int s = A1; s <= H8; ++s)
        ZobristEnPassant[s] = prng_rand(&prng);

    ZobristEnPassant[NB_SQUARE] = prng_rand(&prng);

    ZobristTurn = prng_rand(&prng);
}

uint64_t key(int c, int p, int s)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p, NB_PIECE);
    BOUNDS(s, NB_SQUARE);

    return Zobrist[c][p][s];
}

uint64_t keys(int c, int p, uint64_t sqs)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p, NB_PIECE);

    bitboard_t k = 0;

    while(sqs)
        k ^= key(c, p, bb::pop_lsb(sqs));

    return k;
}

uint64_t castling(bitboard_t castlableRooks)
{
    bitboard_t k = 0;

    while (castlableRooks)
        k ^= ZobristCastling[bb::pop_lsb(castlableRooks)];

    return k;
}

uint64_t en_passant(int s)
{
    assert(unsigned(s) <= NB_SQUARE);

    return ZobristEnPassant[s];
}

uint64_t turn()
{
    return ZobristTurn;
}

}    // namespace zobrist
