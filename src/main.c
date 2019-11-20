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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitboard.h"
#include "eval.h"
#include "gen.h"
#include "htable.h"
#include "platform.h"
#include "position.h"
#include "pst.h"
#include "search.h"
#include "uci.h"
#include "workers.h"

void test(bool perft, int depth)
{
    static const char *fens[] = {
        #include "test.csv"
        NULL
    };

    uint64_t totalNodes = 0, nodes;
    uciChess960 = true;

    memset(&lim, 0, sizeof(lim));
    lim.depth = depth;

    int64_t start = system_msec();

    for (int i = 0; fens[i]; i++) {
        pos_set(&rootPos, fens[i]);
        stack_clear(&rootStack);
        stack_push(&rootStack, rootPos.key);

        if (perft) {
            nodes = gen_perft(&rootPos, depth, 1);
            printf("%s, %" PRIu64 "\n", fens[i], nodes);
        } else {
            puts(fens[i]);
            nodes = search_go();
            puts("");
        }

        totalNodes += nodes;
    }

    if (dbgCnt[0] || dbgCnt[1])
        fprintf(stderr, "dbgCnt[0] = %" PRId64 ", dbgCnt[1] = %" PRId64 "\n", dbgCnt[0], dbgCnt[1]);

    const int64_t elapsed = system_msec() - start;

    printf("Time  : %"PRIu64"ms\n", elapsed);
    printf("Nodes : %"PRIu64"\n", totalNodes);
    printf("NPS   : %.0f\n", totalNodes * 1000.0 / max(elapsed, 1));  // avoid div/0
}

int main(int argc, char **argv)
{
    bb_init();
    pos_init();
    pst_init();
    eval_init();
    search_init();

    if (argc >= 2) {
        if ((!strcmp(argv[1], "perft") || !strcmp(argv[1], "bench")) && argc >= 2) {
            const int depth = argc > 2 ? atoi(argv[2]) : 12;

            if (argc > 3)
                WorkersCount = atoi(argv[3]);

            if (argc > 4)
                uciHash = 1ULL << bb_msb(atoll(argv[4]));  // must be a power of 2

            workers_prepare(WorkersCount);
            hash_prepare(uciHash);
            test(!strcmp(argv[1], "perft"), depth);
        }
    } else {
        workers_prepare(WorkersCount);
        hash_prepare(uciHash);
        uci_loop();
    }

    free(HashTable);
    workers_destroy();
}
