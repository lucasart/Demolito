#pragma once
#include "position.h"

struct PawnEntry {
    uint64_t key;
    eval_t eval;
};

#define NB_PAWN_ENTRY 16384

extern thread_local PawnEntry PawnHash[NB_PAWN_ENTRY];

void eval_init();
int evaluate(const Position *pos);
