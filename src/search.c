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
#include <math.h>
#include <setjmp.h>
#include "eval.h"
#include "htable.h"
#include "move.h"
#include "position.h"
#include "search.h"
#include "smp.h"
#include "sort.h"
#include "uci.h"
#include "zobrist.h"

Position rootPos;
Stack rootStack;
Limits lim;

// Protect thread scheduling decisions
static mtx_t mtxSchedule;

atomic_uint_fast64_t signal;  // bit #i is set if thread #i should abort
static thread_local jmp_buf jbuf;  // exception jump buffer
enum {ABORT_ONE = 1, ABORT_ALL};  // exceptions: abort current or all threads

const int Tempo = 17;

int Contempt = 10;

int draw_score(int ply)
{
    return (ply & 1 ? Contempt : -Contempt) * EP / 100;
}

int Reduction[MAX_DEPTH + 1][MAX_MOVES];

void search_init()
{
    for (int d = 1; d <= MAX_DEPTH; d++)
        for (int c = 1; c < MAX_MOVES; c++)
            Reduction[d][c] = 0.393 * log(d > 31 ? 31 : d) + 0.852 * log(c > 31 ? 31 : c);
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

    int delta = 16;
    int alpha = score - delta;
    int beta = score + delta;

    for ( ; ; delta += delta * 0.959) {
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

void iterate(Worker *worker)
{
    move_t pv[MAX_PLY + 1];
    int volatile score = 0;

    thisWorker = worker;

    for (volatile int depth = 1; depth <= lim.depth; depth++) {
        mtx_lock(&mtxSchedule);

        if (signal == STOP) {
            mtx_unlock(&mtxSchedule);
            return;
        } else
            signal &= ~(1ULL << thisWorker->id);

        // If half of the threads are searching >= depth, then move to the next depth.
        // Special cases where this does not apply:
        // depth == 1: we want all threads to finish depth == 1 asap.
        // depth == lim.depth: there is no next depth.
        if (WorkersCount >= 2 && depth >= 2 && depth < lim.depth) {
            int cnt = 0;

            for (int i = 0; i < WorkersCount; i++)
                cnt += thisWorker != &Workers[i] && Workers[i].depth >= depth;

            if (cnt >= WorkersCount / 2) {
                mtx_unlock(&mtxSchedule);
                continue;
            }
        }

        thisWorker->depth = depth;
        mtx_unlock(&mtxSchedule);

        const int exception = setjmp(jbuf);

        if (exception == 0) {
            score = aspirate(depth, pv, score);

            // Iteration was completed normally. Now we need to see who is working on
            // obsolete iterations, and raise the appropriate signal, to make them move
            // on to the next depth.
            mtx_lock(&mtxSchedule);
            uint64_t s = 0;

            for (int i = 0; i < WorkersCount; i++)
                if (thisWorker != &Workers[i] && Workers[i].depth <= depth)
                    s |= 1ULL << i;

            signal |= s;
            mtx_unlock(&mtxSchedule);
        } else {
            assert(signal & (1ULL << thisWorker->id));
            thisWorker->stack = rootStack;  // Restore an orderly state

            if (exception == ABORT_ONE)
                continue;
            else {
                assert(exception == ABORT_ALL);
                break;
            }
        }

        info_update(&ui, depth, score, smp_nodes(), pv, false);
    }

    // Max depth completed by current thread. All threads should stop.
    mtx_lock(&mtxSchedule);
    signal = STOP;
    mtx_unlock(&mtxSchedule);
}

int64_t search_go(void *dummy)
{
    (void)dummy;  // Silence compiler warning

    struct timespec start;
    static const struct timespec resolution = {0, 5000000};  // 5ms

    timespec_get(&start, TIME_UTC);
    info_create(&ui);
    mtx_init(&mtxSchedule, mtx_plain);
    signal = 0;

    thrd_t threads[WorkersCount];
    smp_new_search();

    for (int i = 0; i < WorkersCount; i++)
        // Start searching thread
        thrd_create(&threads[i], iterate, &Workers[i]);

    do {
        thrd_sleep(&resolution, NULL);

        // Check for search termination conditions, but only after depth 1 has been
        // completed, to make sure we do not return an illegal move.
        if (info_last_depth(&ui) > 0) {
            if (lim.nodes && smp_nodes() >= lim.nodes) {
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

    for (int i = 0; i < WorkersCount; i++)
        thrd_join(threads[i], NULL);

    info_print_bestmove(&ui);
    info_destroy(&ui);
    mtx_destroy(&mtxSchedule);

    const int64_t nodes = smp_nodes();

    return nodes;
}
