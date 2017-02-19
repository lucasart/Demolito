#pragma once
#include "bitboard.h"
#include "types.h"

namespace zobrist {

struct PRNG {
    uint64_t a, b, c, d;
};

void prng_init(PRNG *prng, uint64_t seed);
uint64_t prng_rand(PRNG *prng);

struct GameStack {
    uint64_t keys[MAX_GAME_PLY];
    int idx;
};

void gs_clear(GameStack *gs);
void gs_push(GameStack *gs, uint64_t key);
void gs_pop(GameStack *gs);
uint64_t gs_back(const GameStack *gs);
bool gs_repetition(const GameStack *gs, int rule50);

void init();

uint64_t key(int c, int p, int s);
uint64_t keys(int c, int p, uint64_t sqs);

uint64_t castling(bitboard_t castlableRooks);
uint64_t en_passant(int s);
uint64_t turn();

}    // namespace zobrist
