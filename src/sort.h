#pragma once
#include "gen.h"
#include "workers.h"

void history_update(int16_t *t, int bonus);

typedef struct {
    move_t moves[MAX_MOVES];
    int scores[MAX_MOVES];
    size_t cnt, idx;
} Sort;

void sort_init(Worker *worker, Sort *sort, const Position *pos, int depth, move_t ttMove);
move_t sort_next(Sort *sort, const Position *pos, int *see);
