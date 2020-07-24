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
#include <stdatomic.h>
#include "position.h"
#include "zobrist.h"

enum {
    MAX_DEPTH = 127, MIN_DEPTH = -8,
    MAX_PLY = MAX_DEPTH - MIN_DEPTH + 2,
};

typedef struct {
    int64_t movetime, time, inc;
    uint64_t nodes;
    int depth, movestogo;
    atomic_bool infinite;  // IO thread can change this while Timer thread is checking it
} Limits;

int mated_in(int ply);
int mate_in(int ply);
bool is_mate_score(int score);

extern atomic_bool Stop;  // set this to true to stop the search

extern Position rootPos;
extern ZobristStack rootStack;
extern Limits lim;
extern int Contempt;

void search_init(void);
uint64_t search_go(void);
