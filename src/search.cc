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
#include <thread>
#include <math.h>
#include <setjmp.h>
#include "search.h"
#include "sort.h"
#include "eval.h"
#include "htable.h"
#include "uci.h"
#include "zobrist.h"

Position rootPos;

// Protect thread scheduling decisions
static mtx_t mtxSchedule;

// Set at thread creation, so each thread can know its unique id
thread_local int ThreadId;

// Per thread data
GameStack *gameStacks;
int64_t *nodeCounts;

int64_t count_nodes()
{
    int64_t total = 0;

    for (int i = 0; i < Threads; total += nodeCounts[i++]);

    return total;
}

std::atomic<uint64_t> signal;  // bit #i is set if thread #i should abort
static thread_local jmp_buf jbuf;  // exception jump buffer
enum {ABORT_ONE = 1, ABORT_ALL};  // exceptions: abort current or all threads

const int Tempo = 16;

int Threads = 1;
int Contempt = 10;

int draw_score(int ply)
{
    return (ply & 1 ? Contempt : -Contempt) * EP / 100;
}

int Reduction[32][32];

void search_init()
{
    for (int d = 1; d < 32; d++)
        for (int c = 1; c < 32; c++)
            Reduction[d][c] = 0.4 * log(d) + 0.8 * log(c);
}

// The below is a bit ugly, but the idea is simple. I don't want to maintain 2 separate
// functions: one for search, one for qsearch. So I generate both with a single codebase,
// using the pre-processor. Basically a poor man's template function in C.

int search(const Position *pos, int ply, int depth, int alpha, int beta, move_t pv[]);
int qsearch(const Position *pos, int ply, int depth, int alpha, int beta, move_t pv[]);

#define Qsearch true
#define generic_search qsearch
#include "recurse.h"
#undef generic_search
#undef Qsearch

#define Qsearch false
#define generic_search search
#include "recurse.h"
#undef generic_search
#undef Qsearch

int aspirate(int depth, move_t pv[], int score)
{
    assert(depth > 0);

    if (depth == 1)
        return search(&rootPos, 0, depth, -INF, +INF, pv);

    int delta = 32;
    int alpha = score - delta;
    int beta = score + delta;

    for ( ; ; delta += delta) {
        score = search(&rootPos, 0, depth, alpha, beta, pv);

        if (score <= alpha) {
            beta = (alpha + beta) / 2;
            alpha -= delta;
        } else if (score >= beta) {
            alpha = (alpha + beta) / 2;
            beta += delta;
        } else
            return score;
    }
}

void iterate(const Limits& lim, const GameStack& initialGameStack, int iterations[], int threadId)
{
    ThreadId = threadId;
    move_t pv[MAX_PLY + 1];
    int volatile score = 0;  /* Silence GCC warnings: (1) uninitialized warning (bogus); (2) clobber
        warning due to longjmp (technically correct but inconsequential) */

    memset(PawnHash, 0, sizeof(PawnHash));
    memset(HistoryTable, 0, sizeof(HistoryTable));

    for (int depth = 1; depth <= lim.depth; depth++) {
        mtx_lock(&mtxSchedule);

        if (signal == STOP) {
            mtx_unlock(&mtxSchedule);
            return;
        } else
            signal &= ~(1ULL << ThreadId);

        // If half of the threads are searching >= depth, then move to the next iteration.
        // Special cases where this does not apply:
        // depth == 1: we want all threads to finish depth == 1 asap.
        // depth == lim.depth: there is no next iteration.
        if (Threads >= 2 && depth >= 2 && depth < lim.depth) {
            int cnt = 0;

            for (int i = 0; i < Threads; i++)
                cnt += i != ThreadId && iterations[i] >= depth;

            if (cnt >= Threads / 2) {
                mtx_unlock(&mtxSchedule);
                continue;
            }
        }

        iterations[ThreadId] = depth;

        if (signal == STOP) {
            mtx_unlock(&mtxSchedule);
            return;
        }

        signal &= ~(1ULL << ThreadId);
        mtx_unlock(&mtxSchedule);

        const int exception = setjmp(jbuf);

        if (exception == 0) {
            score = aspirate(depth, pv, score);

            // Iteration was completed normally. Now we need to see who is working on
            // obsolete iterations, and raise the appropriate signal, to make them move
            // on to the next iteration.
            mtx_lock(&mtxSchedule);
            uint64_t s = 0;

            for (int i = 0; i < Threads; i++)
                if (i != ThreadId && iterations[i] <= depth)
                    s |= 1ULL << i;

            signal |= s;
            mtx_unlock(&mtxSchedule);
        } else {
            assert(signal & (1ULL << ThreadId));
            gameStacks[ThreadId] = initialGameStack;  // Restore an orderly state

            if (exception == ABORT_ONE)
                continue;
            else {
                assert(exception == ABORT_ALL);
                break;
            }
        }

        info_update(&ui, depth, score, count_nodes(), pv);
    }

    // Max depth completed by current thread. All threads should stop.
    mtx_lock(&mtxSchedule);
    signal = STOP;
    mtx_unlock(&mtxSchedule);
}

int64_t search_go(const Limits& lim, const GameStack& initialGameStack)
{
    struct timespec start;
    static const struct timespec resolution = {0, 5000000};  // 5ms

    clock_gettime(CLOCK_MONOTONIC, &start);  // FIXME: POSIX only
    info_create(&ui);
    mtx_init(&mtxSchedule, mtx_plain);
    signal = 0;

    int *iterations = (int *)calloc(Threads, sizeof(int));  // FIXME: C++ needs cast
    gameStacks = (GameStack *)malloc(Threads * sizeof(GameStack));  // FIXME: C++ needs cast
    nodeCounts = (int64_t *)calloc(Threads, sizeof(int64_t));  // FIXME: C++ needs cast

    std::thread threads[Threads];

    for (int i = 0; i < Threads; i++) {
        // Initialize per-thread data
        gameStacks[i] = initialGameStack;
        nodeCounts[i] = 0;

        // Start searching thread
        threads[i] = std::thread(iterate, std::cref(lim), std::cref(initialGameStack), iterations, i);
    }

    do {
        nanosleep(&resolution, NULL);  // FIXME: POSIX only

        // Check for search termination conditions, but only after depth 1 has been
        // completed, to make sure we do not return an illegal move.
        if (info_last_depth(&ui) > 0) {
            if (lim.nodes && count_nodes() >= lim.nodes) {
                mtx_lock(&mtxSchedule);
                signal = STOP;
                mtx_unlock(&mtxSchedule);
            } else if (lim.movetime && elapsed_msec(&start) >= lim.movetime) {
                mtx_lock(&mtxSchedule);
                signal = STOP;
                mtx_unlock(&mtxSchedule);
            }
        }
    } while (signal != STOP);

    for (int i = 0; i < Threads; i++)
        threads[i].join();

    info_print_bestmove(&ui);
    info_destroy(&ui);
    mtx_destroy(&mtxSchedule);

    const int64_t nodes = count_nodes();

    free(gameStacks);
    free(nodeCounts);
    free(iterations);

    return nodes;
}
