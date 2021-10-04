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
#include "platform.h"
#include "search.h"
#include "workers.h"
#include <stdlib.h>

Worker *Workers = NULL;
size_t WorkersCount = 1;

static void __attribute__((destructor)) workers_free(void) { free(Workers); }

void workers_clear() {
    for (size_t i = 0; i < WorkersCount; i++) {
        // Clear Workers[i] except .seed, which must be preserved
        uint64_t saveSeed = Workers[i].seed;
        Workers[i] = (Worker){.seed = saveSeed};
    }
}

void workers_prepare(size_t count) {
    Workers = realloc(Workers, count * sizeof(Worker));
    WorkersCount = count;

    for (size_t i = 0; i < count; i++)
        Workers[i].seed = (uint64_t)system_msec() + i;

    workers_clear();
}

void workers_new_search() {
    for (size_t i = 0; i < WorkersCount; i++) {
        Workers[i].stack = rootStack;
        Workers[i].nodes = 0;
    }
}

uint64_t workers_nodes() {
    uint64_t total = 0;

    for (size_t i = 0; i < WorkersCount; i++)
        total += Workers[i].nodes;

    return total;
}
