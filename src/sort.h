#pragma once
#include "gen.h"

void history_update(int c, move_t m, int bonus);

struct Sort {
    move_t moves[MAX_MOVES];
    int scores[MAX_MOVES];
    size_t cnt, idx;
};

void sort_init(Sort *s, const Position *pos, int depth, move_t ttMove);
move_t sort_next(Sort *s, const Position *pos, int *see);
