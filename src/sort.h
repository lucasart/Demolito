/*
 * Demolito, a UCI chess engine. Copyright 2015-2020 lucasart.
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
