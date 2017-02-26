#pragma once
#include "gen.h"

namespace search {

struct History {
    int table[NB_COLOR][NB_SQUARE][NB_SQUARE];
};

void history_update(int c, Move m, int bonus);

extern thread_local History H;

struct Sort {
    Sort(const Position *pos, int depth, move_t ttMove);

    move_t moves[MAX_MOVES];
    int scores[MAX_MOVES];
    size_t cnt, idx;
};

Move sort_next(Sort *s, const Position *pos, int *see);

}    // namespace search
