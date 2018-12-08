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
#include "smp.h"
#include "uci.h"

uint64_t test(bool perft, int depth, int threads, int hash)
{
    static const char *fens[] = {
        #include "test.csv"
        NULL
    };

    hash_resize(hash);
    uint64_t result = 0, nodes;
    smp_resize(threads);
    uciChess960 = true;

    memset(&lim, 0, sizeof(lim));
    lim.depth = depth;

    int64_t start = system_msec();

    for (int i = 0; fens[i]; i++) {
        puts(fens[i]);
        pos_set(&rootPos, fens[i]);
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

int main(int argc, char **argv)
{
    bb_init();
    pos_init();
    pst_init();
    eval_init();
    search_init();
    smp_resize(1);

    if (argc >= 2) {
        if ((!strcmp(argv[1], "perft") || !strcmp(argv[1], "search")) && argc >= 3) {
            const int depth = atoi(argv[2]);
            const int threads = argc > 3 ? atoi(argv[3]) : 1;
            const int hash = argc > 4 ? atoi(argv[4]) : 2;
            const uint64_t nodes = test(!strcmp(argv[1], "perft"), depth, threads, hash);
            fprintf(stderr, "total = %" PRIu64 "\n", nodes);
        }
    } else
        uci_loop();

    free(HashTable);
    smp_destroy();
}
