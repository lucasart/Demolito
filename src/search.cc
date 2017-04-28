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

std::atomic<uint64_t> signal;    // signal: bit #i is set if thread #i should stop
enum Abort {
    ABORT_NEXT,    // current thread aborts the current iteration to be scheduled to the next one
    ABORT_STOP    // current thread aborts the current iteration to stop iterating completely
};

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
            Reduction[d][c] = .4*log(d) + .8*log(c);
}

template<bool Qsearch = false>
int recurse(const Position *pos, int ply, int depth, int alpha, int beta, move_t pv[])
{
    assert(Qsearch == (depth <= 0));
    assert(gs_back(&gameStacks[ThreadId]) == pos->key);
    assert(alpha < beta);

    const bool pvNode = beta > alpha + 1;
    const int oldAlpha = alpha;
    const int us = pos->turn;
    int bestScore = -INF;
    move_t bestMove = 0;
    int score;
    Position nextPos;

    if (!Qsearch) {
        const uint64_t s = signal.load(std::memory_order_relaxed);

        if (s) {
            if (s == STOP)
                throw ABORT_STOP;
            else if (s & (1ULL << ThreadId))
                throw ABORT_NEXT;
        }
    }

    move_t childPv[MAX_PLY - ply];

    if (pvNode)
        pv[0] = 0;

    if (ply > 0 && (gs_repetition(&gameStacks[ThreadId], pos->rule50)
                    || pos_insufficient_material(pos)))
        return draw_score(ply);

    // TT probe
    HashEntry he;
    int staticEval, refinedEval;

    if (hash_read(pos->key, &he)) {
        he.score = score_from_hash(he.score, ply);

        if (he.depth >= depth && !pvNode && ((he.score <= alpha && he.bound >= EXACT) || (he.score >= beta
                                             && he.bound <= EXACT)))
            return he.score;

        if (!Qsearch && he.depth <= 0)
            he.move = 0;

        refinedEval = staticEval = he.eval;

        if ((he.score > refinedEval && he.bound <= EXACT)
                || (he.score < refinedEval && he.bound >= EXACT))
            refinedEval = he.score;
    } else {
        he.move = 0;
        refinedEval = staticEval = pos->checkers ? -INF : evaluate(pos) + Tempo;
    }

    // At Root, ensure that the last best move is searched first. This is not guaranteed,
    // as the TT entry could have got overriden by other search threads.
    if (!Qsearch && ply == 0 && info_last_depth(&ui) > 0)
        he.move = info_best(&ui);

    nodeCounts[ThreadId]++;

    if (ply >= MAX_PLY)
        return refinedEval;

    // Razoring
    if (!Qsearch && depth <= 3 && !pos->checkers && !pvNode) {
        static const int RazorMargin[] = {0, P*3/2, P*5/2, P*7/2};
        const int lbound = alpha - RazorMargin[depth];

        if (refinedEval <= lbound) {
            score = recurse<true>(pos, ply, 0, lbound, lbound+1, childPv);

            if (score <= lbound)
                return score;
        }
    }

    // Null search
    if (!Qsearch && depth >= 2 && !pvNode
            && staticEval >= beta && pos->pieceMaterial[us].eg) {
        const int nextDepth = depth - (2 + depth/3) - (refinedEval >= beta+P);
        pos_switch(&nextPos, pos);
        gs_push(&gameStacks[ThreadId], nextPos.key);
        score = nextDepth <= 0
                ? -recurse<true>(&nextPos, ply+1, nextDepth, -beta, -(beta-1), childPv)
                : -recurse(&nextPos, ply+1, nextDepth, -beta, -(beta-1), childPv);
        gs_pop(&gameStacks[ThreadId]);

        if (score >= beta)
            return score >= mate_in(MAX_PLY) ? beta : score;
    }

    // QSearch stand pat
    if (Qsearch && !pos->checkers) {
        bestScore = refinedEval;

        if (bestScore > alpha) {
            alpha = bestScore;

            if (bestScore >= beta)
                return bestScore;
        }
    }

    // Generate and score moves
    Sort s;
    sort_init(&s, pos, depth, he.move);

    int moveCount = 0, lmrCount = 0;
    move_t currentMove;

    // Move loop
    while ((s.idx != s.cnt) && alpha < beta) {
        int see;
        currentMove = sort_next(&s, pos, &see);

        if (!move_is_legal(pos, currentMove))
            continue;

        moveCount++;

        // Prune losing captures in the qsearch
        if (Qsearch && see < 0 && !pos->checkers)
            continue;

        // SEE proxy tells us we're unlikely to beat alpha
        if (Qsearch && !pos->checkers && staticEval + P/2 <= alpha && see <= 0)
            continue;

        // Play move
        pos_move(&nextPos, pos, currentMove);

        // Prune losing captures in the search, near the leaves
        if (!Qsearch && depth <= 4 && see < 0 && !pvNode && !pos->checkers && !nextPos.checkers
                && !move_is_capture(pos, currentMove))
            continue;

        gs_push(&gameStacks[ThreadId], nextPos.key);

        const int ext = see >= 0 && nextPos.checkers;
        const int nextDepth = depth - 1 + ext;

        // Recursion
        if (Qsearch || nextDepth <= 0) {
            // Qsearch recursion (plain alpha/beta)
            if (depth <= MIN_DEPTH && !pos->checkers) {
                score = staticEval + see;    // guard against QSearch explosion

                if (pvNode)
                    childPv[0] = 0;
            } else
                score = -recurse<true>(&nextPos, ply+1, nextDepth, -beta, -alpha, childPv);
        } else {
            // Search recursion (PVS + Reduction)
            if (moveCount == 1)
                score = -recurse(&nextPos, ply+1, nextDepth, -beta, -alpha, childPv);
            else {
                int reduction = see < 0 || !move_is_capture(pos, currentMove);

                if (!move_is_capture(pos, currentMove)) {
                    lmrCount++;
                    reduction = Reduction[min(31, nextDepth)][min(31, lmrCount)];
                    assert(nextDepth >= 1);
                    assert(lmrCount >= 1);
                }

                // Reduced depth, zero window
                score = nextDepth - reduction <= 0
                        ? -recurse<true>(&nextPos, ply+1, nextDepth - reduction, -alpha-1, -alpha, childPv)
                        : -recurse(&nextPos, ply+1, nextDepth - reduction, -alpha-1, -alpha, childPv);

                // Fail high: re-search zero window at full depth
                if (reduction && score > alpha)
                    score = -recurse(&nextPos, ply+1, nextDepth, -alpha-1, -alpha, childPv);

                // Fail high at full depth for pvNode: re-search full window
                if (pvNode && alpha < score && score < beta)
                    score = -recurse(&nextPos, ply+1, nextDepth, -beta, -alpha, childPv);
            }
        }

        // Undo move
        gs_pop(&gameStacks[ThreadId]);

        // New best score
        if (score > bestScore) {
            bestScore = score;

            // New alpha
            if (score > alpha) {
                alpha = score;
                bestMove = currentMove;

                if (pvNode) {
                    pv[0] = currentMove;

                    for (int i = 0; i < MAX_PLY - ply; i++)
                        if (!(pv[i + 1] = childPv[i]))
                            break;

                    if (!Qsearch && ply == 0 && info_last_depth(&ui) > 0)
                        info_update(&ui, depth, score, count_nodes(), pv, true);
                }
            }
        }
    }

    // No legal move: mated or stalemated
    if ((!Qsearch || pos->checkers) && !moveCount)
        return pos->checkers ? mated_in(ply) : draw_score(ply);

    // Update History
    if (!Qsearch && alpha > oldAlpha && !move_is_capture(pos, bestMove))
        for (size_t i = 0; i < s.idx; i++) {
            const int bonus = depth * depth;
            history_update(us, s.moves[i], s.moves[i] == bestMove ? bonus : -bonus);
        }

    // TT write
    he.bound = bestScore <= oldAlpha ? UBOUND : bestScore >= beta ? LBOUND : EXACT;
    he.score = score_to_hash(bestScore, ply);
    he.eval = pos->checkers ? -INF : staticEval;
    he.depth = depth;
    he.move = bestMove;
    he.keyXorData = pos->key ^ he.data;
    hash_write(pos->key, &he);

    return bestScore;
}

int aspirate(int depth, move_t pv[], int score)
{
    if (depth <= 1) {
        assert(depth == 1);
        return recurse(&rootPos, 0, depth, -INF, +INF, pv);
    }

    int delta = 32;
    int alpha = score - delta;
    int beta = score + delta;

    for ( ; ; delta += delta) {
        score = recurse(&rootPos, 0, depth, alpha, beta, pv);

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
    int score = 0;  // Silence bogus GCC warning (score may be uninitialized)

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

        try {
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
        } catch (const Abort e) {
            assert(signal & (1ULL << ThreadId));
            gameStacks[ThreadId] = initialGameStack;    // Restore an orderly state

            if (e == ABORT_STOP)
                break;
            else {
                assert(e == ABORT_NEXT);
                continue;
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
