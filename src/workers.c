/*
 * Demolito, a UCI chess engine.
 * Copyright 2015 lucasart.
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
#include <stdlib.h>
#include <string.h>
#include "workers.h"
#include "search.h"

Worker *Workers = NULL;
int WorkersCount = 1;

static void __attribute__((destructor)) workers_free()
{
    free(Workers);
}

void workers_clear()
{
    for (int i = 0; i < WorkersCount; i++)
        memset(&Workers[i], 0, sizeof(Workers[i]));
}

void workers_prepare(int count)
{
    Workers = realloc(Workers, count * sizeof(Worker));
    WorkersCount = count;
    workers_clear();
}

void workers_new_search()
{
    for (int i = 0; i < WorkersCount; i++) {
        Workers[i].stack = rootStack;
        Workers[i].nodes = 0;
        Workers[i].depth = 0;
    }
}

int64_t workers_nodes()
{
    int64_t total = 0;

    for (int i = 0; i < WorkersCount; i++)
        total += Workers[i].nodes;

    return total;
}
