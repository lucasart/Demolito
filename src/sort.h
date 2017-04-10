#pragma once
#include "gen.h"

extern thread_local int HistoryTable[NB_COLOR][NB_SQUARE][NB_SQUARE];

void history_update(int c, move_t m, int bonus);

struct Sort {
    Sort(const Position *pos, int depth, move_t ttMove);

    move_t moves[MAX_MOVES];
    int scores[MAX_MOVES];
    size_t cnt, idx;
};

move_t sort_next(Sort *s, const Position *pos, int *see);
