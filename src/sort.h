#pragma once
#include "gen.h"

void history_update(int16_t *t, int16_t bonus);

typedef struct {
    move_t moves[MAX_MOVES];
    int scores[MAX_MOVES];
    size_t cnt, idx;
} Sort;

void sort_init(Worker *worker, Sort *s, const Position *pos, int depth, move_t ttMove);
move_t sort_next(Sort *s, const Position *pos, int *see);
