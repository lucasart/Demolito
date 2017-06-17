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
#include "bitboard.h"
#include "eval.h"
#include "htable.h"
#include "pst.h"
#include "search.h"
#include "smp.h"
#include "test.h"
#include "uci.h"
#include "zobrist.h"

int main(int argc, char **argv)
{
    bb_init();
    zobrist_init();
    pst_init();
    eval_init();
    search_init();
    smp_resize(1);

    if (argc >= 2) {
        if ((!strcmp(argv[1], "perft") || !strcmp(argv[1], "search")) && argc >= 4) {
            const int depth = atoi(argv[2]), threads = atoi(argv[3]);
            const uint64_t nodes = test_search(!strcmp(argv[1], "perft"), depth, threads);
            printf("total = %" PRIu64 "\n", nodes);
        }
    } else
        uci_loop();

    free(HashTable);
    smp_destroy();
}
