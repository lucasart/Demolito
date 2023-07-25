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
#include "bitboard.h"
#include "eval.h"
#include "htable.h"
#include "platform.h"
#include "position.h"
#include "search.h"
#include "uci.h"
#include "workers.h"
#include <stdlib.h>
#include <string.h>

static void bench(int depth) {
    static const char *fens[] = {
#include "test.csv"
        NULL};

    uint64_t nodes = 0;
    uciChess960 = true;

    lim = (Limits){0};
    lim.depth = depth;

    int64_t start = system_msec();

    for (int i = 0; fens[i]; i++) {
        pos_set(&rootPos, fens[i]);
        zobrist_clear(&rootStack);
        zobrist_push(&rootStack, rootPos.key);

        puts(fens[i]);
        nodes += search_go();
        puts("");
    }

    if (dbgCnt[0] || dbgCnt[1])
        printf("dbgCnt[0] = %" PRId64 ", dbgCnt[1] = %" PRId64 "\n", dbgCnt[0], dbgCnt[1]);

    const int64_t elapsed = system_msec() - start;

    printf("time  : %" PRIu64 "ms\n", elapsed);
    printf("nodes : %" PRIu64 "\n", nodes); // total nodes = functionality signature
    printf("nps   : %.0f\n", (double)nodes * 1000.0 / (double)max(elapsed, 1)); // avoid div/0
}

int main(int argc, char **argv) {
    eval_init();
    search_init();

    if (argc >= 2) {
        if (!strcmp(argv[1], "bench")) {
            const int depth = argc > 2 ? atoi(argv[2]) : 12;

            if (argc > 3)
                WorkersCount = (size_t)atoll(argv[3]);

            if (argc > 4)
                uciHash = 1ULL << bb_msb((uint64_t)atoll(argv[4])); // must be a power of 2

            workers_prepare(WorkersCount);
            hash_prepare(uciHash);
            bench(depth);
        } else
            puts("Syntax: demolito [bench [depth [threads [hash]]]]");
    } else {
        workers_prepare(WorkersCount);
        hash_prepare(uciHash);
        uci_loop();
    }
}
