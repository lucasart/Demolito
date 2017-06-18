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
#include "position.h"
#include "search.h"
#include "smp.h"
#include "test.h"

uint64_t test_search(bool perft, int depth, int threads)
{
    const char *fens[] = {
        // Chess960 (both KQ and AH castling notations)
        "br1kq1r1/pppppp1p/3nn1p1/8/6P1/1P1NN3/P1PPPP1P/R2KQ1RB b Kkq - 0 5",
        "b1rk2r1/p2pp1qp/1p1np1p1/2p5/6PP/1P1NNPQ1/P1PPP3/2RK2R1 b Gg - 0 11",
        "2rk4/pb2pr1p/1p2p1p1/2pp2P1/2P4P/PP2NP2/3PP3/2RK2R1 w G - 0 18",
        "r7/1bk1p2p/1p2p1p1/1pp2rP1/P1Pp2NP/3P1P2/3KP3/2R3R1 w - - 0 24",
        "r7/1b2p2p/1p1k2p1/1Pp1prP1/2Pp2NP/3P1P2/2R1PK2/6R1 w - - 0 28",
        "5r2/2k1p2p/1p4pN/1Pp3P1/2Ppb2P/r2P2K1/3RP3/4R3 b - - 1 35",
        "8/1bk2r1p/1p2RNp1/1Pp3P1/2Pp1K1P/3P4/r7/3R4 b - - 2 41",
        "8/1bk2r2/1p3RpP/1Pp5/2Pp3P/3P4/7K/3R4 b - - 0 45",
        "1k6/7R/1p4p1/1Pp2b2/2Pp3P/3P2K1/8/8 w - - 1 52",
        "8/2k5/1p4p1/1Pp5/2b2K1P/3R4/8/8 w - - 0 59",
        "8/8/1p1k4/2p3R1/7P/5b2/3K4/8 w - - 7 64",
        "8/8/1R6/1pp5/2k5/8/3K4/8 b - - 1 70",
        "8/8/8/2p5/1p3k2/1K6/1R6/8 b - - 9 75",
        "bqr1nkr1/p2ppp1p/1p2n1p1/2p5/8/1P1NN3/P1PPPPPP/Q1R2RKB w cg - 0 6",
        "2r2kr1/pq1ppp2/4nn2/1pp1N1pp/2P5/1P1N2P1/PQ1PPP1P/2R2RK1 b kq - 2 12",
        "2r2kr1/pq2pp2/3p1n2/1pp3pp/2P5/1P1NR1P1/PQ1P1P1P/5RK1 w cg - 2 17",
        "2r2kr1/p4p2/3pp3/1qpn1R2/6pp/1P1N2P1/P1QP1P1P/4R1K1 w kq - 0 22",
        "2r3r1/p4pk1/3ppn1R/2p5/6p1/1P1N1QPp/P2P3P/4R1K1 b - - 0 27",
        "7r/2r2pk1/3p4/p1p5/6R1/1P1N1KPp/P2P3P/8 b - - 1 34",
        "7r/r4p2/2kp4/p7/3R4/1P2NKPp/P6P/8 b - - 4 40",
        "8/7r/2k2p2/p2p1N2/6P1/1P1R2Kp/P6P/4r3 w - - 4 46",
        "8/3r4/1k3p2/p4N2/3p2P1/1P4Kp/P1R4P/7r w - - 0 51",
        "8/3r4/5p2/p7/2Nk2P1/1P1p2Kp/P2R3P/6r1 w - - 12 58",
        "8/3r4/5p2/p7/6P1/PP3K1p/2k4P/3N4 b - - 0 64",
        // Standard chess
        "r1bqk3/ppp1pp2/2n2npr/3p4/3P4/2N2N2/PPP1PPBP/R2QK2R b KQq - 1 6",
        "r3k3/ppp1pp2/3q1np1/3pNb1r/1n1P1P2/2N1P2P/PPP3B1/R2QK2R w KQq - 1 10",
        "r4k2/1pp1pp2/p1nq1np1/3pNb2/3P1P1r/P1N1PB1P/1PP2Q2/2R1K2R b K - 6 15",
        "r4k2/1pp1pp1r/p1nq2p1/4N3/3PpP1P/P3P3/1PP3Q1/2R1K2R b K - 1 19",
        "r4k2/1pp1pp2/p5p1/4Pr2/3PR2P/P3P3/1PP5/2R3K1 b - - 0 24",
        "r7/1pp1kp2/4p1p1/4Pr2/1P1PP1RP/6K1/2P5/2R5 b - - 0 30",
        "8/1p2kp1r/2p1p1p1/4P3/1P1PP2P/2P3K1/r4R2/5R2 b - - 0 34",
        "8/4kp1r/1pp1p1p1/4P3/1P1PP1KP/r1P5/2R5/5R2 b - - 5 38",
        "r7/4kp1r/2p1p1p1/1p2P3/1P1PP1KP/2P5/7R/5R2 b - - 9 43",
        "r7/4kp2/2p1p1p1/1p2P2r/1P1PP2P/2P5/2R2K2/7R w - - 22 49",
        "r7/5p2/2p1p1pk/1p2P3/1P1PP2P/2P5/1K6/6R1 b - - 6 90",
        "8/3r1p2/8/1pKpP1Pk/1P3R2/2P5/8/8 b - - 0 105",
        "8/5R2/8/1K4k1/1P6/2r5/8/8 w - - 0 111",
        "r1b2k1r/pp2nppp/2p3qn/3pp3/8/1PN1P2P/P1PPBPP1/R2QK1NR w KQ - 2 7",
        "r1b2k1r/1p2n1pp/p1q4n/1Npp4/4p1P1/1PN1P3/P1PPBP2/R2QK1R1 w Q - 0 13",
        "r1b2k1r/4nnpp/p4q2/1pp5/3ppPP1/NP2P3/PNPPBK2/R2Q2R1 w - - 3 18",
        "r1b2k1r/5n2/p4q2/1ppn1Pp1/3pp1p1/NP2P3/P1PPBK2/1RQN2R1 w - - 0 22",
        "r1bqk2r/pppp1ppp/5n2/4b3/4P3/P1N5/1PP2PPP/R1BQKB1R w KQkq - 0 5",
        "r1bqr1k1/pp1p1ppp/2p5/8/3N1Q2/P2BB3/1PP2PPP/R3K2n b Q - 1 12",
        "r1bq2k1/p4r1p/1pp2pp1/3p4/1P1B3Q/P2B1N2/2P3PP/4R1K1 b - - 2 19",
        "r4qk1/6r1/1p4p1/2ppBbN1/1p5Q/P7/2P3PP/5RK1 w - - 2 25",
        "r7/6k1/1p6/2pp1p2/7Q/8/p1P2K1P/8 w - - 0 32",
        "r3k2r/ppp1pp1p/2nqb1pn/3p4/4P3/2PP4/PP1NBPPP/R2QK1NR w KQkq - 1 5",
        "3r1rk1/1pp1pn1p/p1n1q1p1/3p4/Q3P3/2P5/PP1NBPPP/4RRK1 w - - 0 12",
        "5rk1/1pp1pn1p/p3Brp1/8/1n6/5N2/PP3PPP/2R2RK1 w - - 2 20",
        "8/1p2pk1p/p1p1r1p1/3n4/8/5R2/PP3PPP/4R1K1 b - - 3 27",
        "8/4pk2/1p1r2p1/p1p4p/Pn5P/3R4/1P3PP1/4RK2 w - - 1 33",
        "8/5k2/1pnrp1p1/p1p4p/P6P/4R1PK/1P3P2/4R3 b - - 1 38",
        "8/8/1p1kp1p1/p1pr1n1p/P6P/1R4P1/1P3PK1/1R6 b - - 15 45",
        "8/8/1p1k2p1/p1prp2p/P2n3P/6P1/1P1R1PK1/4R3 b - - 5 49",
        "8/8/1p4p1/p1p2k1p/P2npP1P/4K1P1/1P6/3R4 w - - 6 54",
        "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/6R1 b - - 6 59",
        "8/5k2/1p4p1/p1pK3p/P2n1P1P/6P1/1P6/4R3 b - - 14 63",
        "8/1R6/1p1K1kp1/p6p/P1p2P1P/6P1/1Pn5/8 w - - 0 67",
        NULL
    };

    hash_resize(1);
    uint64_t result = 0, nodes;
    smp_resize(threads);
    smp_new_game();

    Chess960 = true;  // Test positions contain some Chess960 ones

    memset(&lim, 0, sizeof(lim));
    lim.depth = depth;

    int64_t start = system_msec();

    for (int i = 0; fens[i]; i++) {
        pos_set(&rootPos, fens[i]);
        stack_clear(&rootStack);
        stack_push(&rootStack, rootPos.key);
        pos_print(&rootPos);

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

    printf("kn/s: %" PRIu64 "\n", result / (system_msec() - start));

    return result;
}
