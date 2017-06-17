#pragma once
#include "gen.h"

void history_update(Worker *worker, int c, move_t m, int bonus);

typedef struct {
    move_t moves[MAX_MOVES];
    int scores[MAX_MOVES];
    size_t cnt, idx;
} Sort;

void sort_init(Worker *worker, Sort *s, const Position *pos, int depth, move_t ttMove, int ply);
move_t sort_next(Sort *s, const Position *pos, int *see);
