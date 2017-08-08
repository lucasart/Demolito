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
#include "gen.h"
#include "htable.h"
#include "platform.h"
#include "position.h"
#include "search.h"
#include "test.h"

uint64_t test_search(bool perft, int depth, int threads)
{
    const char *fens[] = {
        #include "test.csv"
        NULL
    };

    hash_resize(1);
    uint64_t result = 0, nodes;
    smp_resize(threads);
    smp_new_game();

    memset(&lim, 0, sizeof(lim));
    lim.depth = depth;

    int64_t start = system_msec();

    for (int i = 0; fens[i]; i++) {
        pos_set(&rootPos, fens[i], true);
        stack_clear(&rootStack);
        stack_push(&rootStack, rootPos.key);

        if (perft) {
            nodes = gen_perft(&rootPos, depth, 0);
            printf("perft(%d) = %" PRIu64 "\n", depth, nodes);
        } else
            nodes = search_go();

        puts("");
        result += nodes;
    }

    if (dbgCnt[1])
        printf("dbgCnt[0] = %" PRId64 ", dbgCnt[1] = %" PRId64 "\n", dbgCnt[0], dbgCnt[1]);

    fprintf(stderr, "kn/s: %" PRIu64 "\n", result / (system_msec() - start));

    return result;
}
