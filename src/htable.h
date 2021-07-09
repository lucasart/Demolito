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
#include "position.h"
#include <inttypes.h>
#include <stdbool.h>

enum { LBOUND, EXACT, UBOUND };

typedef struct {
    uint64_t key;
    union {
        uint64_t data;
        struct {
            int16_t score, eval;
            move_t move;
            int8_t depth;
            uint8_t bound : 2, date : 6;
        };
    };
} HashEntry;

void hash_prepare(uint64_t hashMB); // realloc + clear
HashEntry hash_read(uint64_t key, int ply);
void hash_write(uint64_t key, HashEntry *e, int ply);
void hash_prefetch(uint64_t key);

int hash_permille(void);

extern unsigned hashDate;
extern HashEntry *HashTable;
extern size_t HashCount;
