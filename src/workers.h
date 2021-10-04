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
#include "bitboard.h"
#include "search.h"
#include "zobrist.h"
#include <setjmp.h>

enum { NB_PAWN_HASH = 16384, NB_REFUTATION = 1024, NB_FOLLOW_UP = 1024 };

typedef struct {
    uint64_t key;
    bitboard_t passed;
    eval_t eval;
} PawnEntry;

typedef struct {
    PawnEntry pawnHash[NB_PAWN_HASH];
    int16_t history[NB_COLOR][NB_SQUARE][NB_SQUARE];
    int16_t refutationHistory[NB_REFUTATION][NB_PIECE][NB_SQUARE];
    int16_t followUpHistory[NB_FOLLOW_UP][NB_PIECE][NB_SQUARE];
    ZobristStack stack;
    jmp_buf jbuf;
    uint64_t nodes;
    uint64_t seed;
    int eval[MAX_PLY];
} Worker;

extern Worker *Workers;
extern size_t WorkersCount;

void workers_clear(void);
void workers_prepare(size_t count); // realloc + clear

void workers_new_search(void);
uint64_t workers_nodes(void);
