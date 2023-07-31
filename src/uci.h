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
#include "platform.h"
#include "position.h"

#define NB_LEVEL 14

void uci_loop(void);

typedef struct {
    mtx_t mtx;
    int64_t start;
    double variability;
    int lastDepth;
    move_t best, ponder;
} Info;

extern Info ui;
extern int uciLevel;
extern int64_t uciTimeBuffer;
extern bool uciChess960, uciFakeTime;
extern size_t uciHash, uciThreads;

void info_create(Info *info);
void info_destroy(Info *info);

void info_update(Info *info, int depth, int score, uint64_t nodes, move_t pv[], bool partial);
void info_print_bestmove(Info *info);
move_t info_best(Info *info);
int info_last_depth(Info *info);
double info_variability(Info *info);
