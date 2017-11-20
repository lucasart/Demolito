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
#include "eval.h"
#include "htable.h"
#include "move.h"
#include "position.h"
#include "search.h"
#include "sort.h"
#include "uci.h"

Position rootPos;
Stack rootStack;
Limits lim;

// Protect thread scheduling decisions
static mtx_t mtxSchedule;

atomic_uint_fast64_t Signal;  // bit #i is set if thread #i should abort
enum {ABORT_ONE = 1, ABORT_ALL};  // exceptions: abort current or all threads

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
            Reduction[d][c] = 0.403 * log(d > 31 ? 31 : d) + 0.877 * log(c > 31 ? 31 : c);
}

const int Tempo = 17;

static int qsearch(Worker *worker, const Position *pos, int ply, int depth, int alpha, int beta,
    move_t pv[])
{
    assert(depth <= 0);
    assert(stack_back(&worker->stack) == pos->key);
    assert(alpha < beta);

    const bool pvNode = beta > alpha + 1;
    const int oldAlpha = alpha;
    int bestScore = -INF;
    move_t bestMove = 0;
    int score;
    Position nextPos;

    move_t childPv[MAX_PLY - ply];

    if (pvNode)
        pv[0] = 0;

    if (ply > 0 && (stack_repetition(&worker->stack, pos->rule50)
            || pos_insufficient_material(pos)))
        return draw_score(ply);

    // TT probe
    HashEntry he;
    int staticEval, refinedEval;

    if (hash_read(pos->key, &he)) {
        he.score = score_from_hash(he.score, ply);

        if (he.depth >= depth && !pvNode && ((he.score <= alpha && he.bound >= EXACT)
                || (he.score >= beta && he.bound <= EXACT)))
            return he.score;

        refinedEval = staticEval = he.eval;

        if ((he.score > refinedEval && he.bound <= EXACT)
                || (he.score < refinedEval && he.bound >= EXACT))
            refinedEval = he.score;
    } else {
        he.move = 0;
        refinedEval = staticEval = pos->checkers ? -INF : evaluate(worker, pos) + Tempo;
    }

    worker->nodes++;

    if (ply >= MAX_PLY)
        return refinedEval;

    // QSearch stand pat
    if (!pos->checkers) {
        bestScore = refinedEval;

        if (bestScore > alpha) {
            alpha = bestScore;

            if (bestScore >= beta)
                return bestScore;
        }
    }

    // Generate and score moves
    Sort s;
    sort_init(worker, &s, pos, depth, he.move, ply);

    int moveCount = 0;
    move_t currentMove;

    // Move loop
    while ((s.idx != s.cnt) && alpha < beta) {
        int see;
        currentMove = sort_next(&s, pos, &see);

        if (!move_is_legal(pos, currentMove))
            continue;

        moveCount++;

        // Prune losing captures in the qsearch
        if (see < 0 && !pos->checkers)
            continue;

        // SEE proxy tells us we're unlikely to beat alpha
        if (!pos->checkers && staticEval + P / 2 <= alpha && see <= 0)
            continue;

        // Play move
        pos_move(&nextPos, pos, currentMove);

        stack_push(&worker->stack, nextPos.key);

        const int ext = see >= 0 && nextPos.checkers;
        const int nextDepth = depth - 1 + ext;

        // Recursion (plain alpha/beta)
        if (depth <= MIN_DEPTH && !pos->checkers) {
            score = staticEval + see;    // guard against QSearch explosion

            if (pvNode)
                childPv[0] = 0;
        } else
            score = -qsearch(worker, &nextPos, ply + 1, nextDepth, -beta, -alpha, childPv);

        // Undo move
        stack_pop(&worker->stack);

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
                }
            }
        }
    }

    // No legal move: mated or stalemated
    if (pos->checkers && !moveCount)
        return pos->checkers ? mated_in(ply) : draw_score(ply);

    // Return alpha when all moves are pruned
    if (bestScore <= -MATE) {
        assert(bestScore == -INF);
        return alpha;
    }

    // TT write
    he.bound = bestScore <= oldAlpha ? UBOUND : bestScore >= beta ? LBOUND : EXACT;
    he.singular = 0;
    he.score = score_to_hash(bestScore, ply);
    he.eval = pos->checkers ? -INF : staticEval;
    he.depth = depth;
    he.move = bestMove;
    he.date = hash_date;
    he.keyXorData = pos->key ^ he.data;
    hash_write(pos->key, &he);

    return bestScore;
}

static int search(Worker *worker, const Position *pos, int ply, int depth, int alpha, int beta,
    move_t pv[], move_t singularMove)
{
    const int EvalMargin[] = {0, 132, 266, 405, 524, 663};
    const int RazorMargin[] = {0, 227, 455, 502, 853};

    assert(depth > 0);
    assert(stack_back(&worker->stack) == pos->key);
    assert(alpha < beta);

    const bool pvNode = beta > alpha + 1;
    const int oldAlpha = alpha;
    const int us = pos->turn;
    int bestScore = -INF;
    move_t bestMove = 0;
    int score;
    Position nextPos;

    const uint64_t tmp = atomic_load_explicit(&Signal, memory_order_relaxed);

    if (tmp) {
        if (tmp == STOP)
            longjmp(worker->jbuf, ABORT_ALL);
        else if (tmp & (1ULL << worker->id))
            longjmp(worker->jbuf, ABORT_ONE);
    }

    move_t childPv[MAX_PLY - ply];

    if (pvNode)
        pv[0] = 0;

    if (ply > 0 && (stack_repetition(&worker->stack, pos->rule50)
            || pos_insufficient_material(pos)))
        return draw_score(ply);

    // TT probe
    HashEntry he;
    int staticEval, refinedEval;
    const uint64_t key = pos->key ^ singularMove;

    if (hash_read(key, &he)) {
        he.score = score_from_hash(he.score, ply);

        if (he.depth >= depth && !pvNode && ((he.score <= alpha && he.bound >= EXACT)
                || (he.score >= beta && he.bound <= EXACT)))
            return he.score;

        if (he.depth <= 0)
            he.move = 0;

        refinedEval = staticEval = he.eval;

        if ((he.score > refinedEval && he.bound <= EXACT)
                || (he.score < refinedEval && he.bound >= EXACT))
            refinedEval = he.score;
    } else {
        he.move = 0;
        refinedEval = staticEval = pos->checkers ? -INF : evaluate(worker, pos) + Tempo;
    }

    // At Root, ensure that the last best move is searched first. This is not guaranteed,
    // as the TT entry could have got overriden by other search threads.
    if (ply == 0 && info_last_depth(&ui) > 0)
        he.move = info_best(&ui);

    worker->nodes++;

    if (ply >= MAX_PLY)
        return refinedEval;

    // Eval pruning
    if (depth <= 5 && !pos->checkers && !pvNode && pos->pieceMaterial[us].eg
            && refinedEval >= beta + EvalMargin[depth])
        return refinedEval;

    // Razoring
    if (depth <= 4 && !pos->checkers && !pvNode) {
        const int lbound = alpha - RazorMargin[depth];

        if (refinedEval <= lbound) {
            score = qsearch(worker, pos, ply, 0, lbound, lbound + 1, childPv);

            if (score <= lbound)
                return score;
        }
    }

    // Null search
    if (depth >= 2 && !pvNode
            && staticEval >= beta && pos->pieceMaterial[us].eg) {
        const int nextDepth = depth - (2 + depth / 3) - (refinedEval >= beta + 178);
        pos_switch(&nextPos, pos);
        stack_push(&worker->stack, nextPos.key);
        score = nextDepth <= 0
            ? -qsearch(worker, &nextPos, ply + 1, nextDepth, -beta, -(beta - 1), childPv)
            : -search(worker, &nextPos, ply + 1, nextDepth, -beta, -(beta - 1), childPv, 0);
        stack_pop(&worker->stack);

        if (score >= beta)
            return score >= mate_in(MAX_PLY) ? beta : score;
    }

    // Generate and score moves
    Sort s;
    sort_init(worker, &s, pos, depth, he.move, ply);

    int moveCount = 0, lmrCount = 0;
    bool hashMoveWasSingular = false;
    move_t currentMove;

    // Move loop
    while ((s.idx != s.cnt) && alpha < beta) {
        int see;
        currentMove = sort_next(&s, pos, &see);

        if (!move_is_legal(pos, currentMove) || currentMove == singularMove)
            continue;

        moveCount++;

        // Play move
        pos_move(&nextPos, pos, currentMove);

        // Prune losing captures and lated moves, near the leaves
        if (depth <= 4 && !pvNode && !pos->checkers && !nextPos.checkers
                && (see < 0 || (depth <= 2 && moveCount >= 1 + 4 * depth))
                && !move_is_capture(pos, currentMove))
            continue;

        // Search extension
        int ext = 0;

        if (currentMove == he.move && ply > 0 && depth >= 6 && he.bound <= EXACT && he.depth >= depth - 4) {
            // See if the Hash Move is Singular, and should be extended
            // Try to retrieve from HT first
            ext = he.depth >= depth && he.singular;

            if (!ext) {
                // Otherwise do a Singular Extension Search
                const int lbound = he.score - 2 * depth;

                if (abs(lbound) < MATE) {
                    score = search(worker, pos, ply, depth - 4, lbound, lbound + 1, childPv, currentMove);
                    ext = score <= lbound;
                }
            }

            hashMoveWasSingular = ext;
        } else
            // Check extension
            ext = see >= 0 && nextPos.checkers;

        const int nextDepth = depth - 1 + ext;

        stack_push(&worker->stack, nextPos.key);

        // Recursion
        if (nextDepth <= 0)
            score = -qsearch(worker, &nextPos, ply + 1, nextDepth, -beta, -alpha, childPv);
        else {
            // Search recursion (PVS + Reduction)
            if (moveCount == 1)
                score = -search(worker, &nextPos, ply + 1, nextDepth, -beta, -alpha, childPv, 0);
            else {
                int reduction = see < 0 || !move_is_capture(pos, currentMove);

                if (!move_is_capture(pos, currentMove)) {
                    lmrCount++;
                    assert(1 <= nextDepth && nextDepth <= MAX_DEPTH);
                    assert(1 <= lmrCount && lmrCount <= MAX_MOVES);
                    reduction = Reduction[nextDepth][lmrCount];
                }

                // Reduced depth, zero window
                score = nextDepth - reduction <= 0
                        ? -qsearch(worker, &nextPos, ply + 1, nextDepth - reduction, -(alpha + 1), -alpha, childPv)
                        : -search(worker, &nextPos, ply + 1, nextDepth - reduction, -(alpha + 1), -alpha, childPv, 0);

                // Fail high: re-search zero window at full depth
                if (reduction && score > alpha)
                    score = -search(worker, &nextPos, ply + 1, nextDepth, -(alpha + 1), -alpha, childPv, 0);

                // Fail high at full depth for pvNode: re-search full window
                if (pvNode && alpha < score && score < beta)
                    score = -search(worker, &nextPos, ply + 1, nextDepth, -beta, -alpha, childPv, 0);
            }
        }

        // Undo move
        stack_pop(&worker->stack);

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

                    if (ply == 0 && info_last_depth(&ui) > 0)
                        info_update(&ui, depth, score, smp_nodes(), pv, true);
                }
            }
        }
    }

    // No legal move: mated or stalemated
    if (!moveCount)
        return singularMove ? alpha : pos->checkers ? mated_in(ply) : draw_score(ply);

    // Return alpha when all moves are pruned
    if (bestScore <= -MATE) {
        assert(bestScore == -INF);
        return alpha;
    }

    // Update move sorting statistics
    if (alpha > oldAlpha && !singularMove && !move_is_capture(pos, bestMove)) {
        for (size_t i = 0; i < s.idx; i++) {
            const int bonus = depth * depth;
            history_update(worker, us, s.moves[i], s.moves[i] == bestMove ? bonus : -bonus);
        }

        worker->refutation[stack_move_key(&worker->stack) & (NB_REFUTATION - 1)] = bestMove;
        worker->killers[ply] = bestMove;
    }

    // TT write
    he.bound = bestScore <= oldAlpha ? UBOUND : bestScore >= beta ? LBOUND : EXACT;
    he.singular = hashMoveWasSingular;
    he.score = score_to_hash(bestScore, ply);
    he.eval = pos->checkers ? -INF : staticEval;
    he.depth = depth;
    he.move = bestMove;
    he.date = hash_date;
    he.keyXorData = key ^ he.data;
    hash_write(key, &he);

    return bestScore;
}

static int aspirate(Worker *worker, int depth, move_t pv[], int score)
{
    assert(depth > 0);

    if (depth == 1)
        return search(worker, &rootPos, 0, depth, -INF, +INF, pv, 0);

    int delta = 15;
    int alpha = score - delta;
    int beta = score + delta;

    for ( ; ; delta += delta * 0.876) {
        score = search(worker, &rootPos, 0, depth, alpha, beta, pv, 0);

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

static void iterate(Worker *worker)
{
    move_t pv[MAX_PLY + 1];
    int volatile score = 0;

    for (volatile int depth = 1; depth <= lim.depth; depth++) {
        mtx_lock(&mtxSchedule);

        if (Signal == STOP) {
            mtx_unlock(&mtxSchedule);
            return;
        } else
            Signal &= ~(1ULL << worker->id);

        // If half of the threads are searching >= depth, then move to the next depth.
        // Special cases where this does not apply:
        // depth == 1: we want all threads to finish depth == 1 asap.
        // depth == lim.depth: there is no next depth.
        if (WorkersCount >= 2 && depth >= 2 && depth < lim.depth) {
            int cnt = 0;

            for (int i = 0; i < WorkersCount; i++)
                cnt += worker != &Workers[i] && Workers[i].depth >= depth;

            if (cnt >= WorkersCount / 2) {
                mtx_unlock(&mtxSchedule);
                continue;
            }
        }

        worker->depth = depth;
        mtx_unlock(&mtxSchedule);

        const int exception = setjmp(worker->jbuf);

        if (exception == 0) {
            score = aspirate(worker, depth, pv, score);

            // Iteration was completed normally. Now we need to see who is working on
            // obsolete iterations, and raise the appropriate Signal, to make them move
            // on to the next depth.
            mtx_lock(&mtxSchedule);
            uint64_t s = 0;

            for (int i = 0; i < WorkersCount; i++)
                if (worker != &Workers[i] && Workers[i].depth <= depth)
                    s |= 1ULL << i;

            Signal |= s;
            mtx_unlock(&mtxSchedule);
        } else {
            worker->stack.idx = rootStack.idx;  // Restore stack position

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
    Signal = STOP;
}

int64_t search_go()
{
    int64_t start = system_msec();

    info_create(&ui);
    mtx_init(&mtxSchedule, mtx_plain);
    Signal = 0;

    hash_date++;
    thrd_t threads[WorkersCount];
    smp_new_search();

    int minTime = 0, maxTime = 0;  // Silence bogus gcc warning (maybe uninitialized)

    if (!lim.movetime && (lim.time || lim.inc)) {
        const int movesToGo = lim.movestogo ? lim.movestogo : 26;
        const int remaining = (movesToGo - 1) * lim.inc + lim.time;
        minTime = 0.57 * remaining / movesToGo;
        maxTime = 2.21 * remaining / movesToGo;

        minTime = min(minTime, lim.time - TimeBuffer);
        maxTime = min(maxTime, lim.time - TimeBuffer);
    }

    for (int i = 0; i < WorkersCount; i++)
        // Start searching thread
        thrd_create(&threads[i], iterate, &Workers[i]);

    do {
        thrd_sleep(5);

        // Check for search termination conditions, but only after depth 1 has been
        // completed, to make sure we do not return an illegal move.
        if (info_last_depth(&ui) > 0) {
            if ((lim.movetime && system_msec() - start >= lim.movetime - TimeBuffer)
                    || (lim.nodes && smp_nodes() >= lim.nodes))
                Signal = STOP;
            else if (lim.time || lim.inc) {
                const double x = 1 / (1 + exp(-info_variability(&ui)));
                const int64_t t = x * maxTime + (1 - x) * minTime;

                if (system_msec() - start >= t)
                    Signal = STOP;
            }
        }
    } while (Signal != STOP);

    for (int i = 0; i < WorkersCount; i++)
        thrd_join(threads[i], NULL);

    info_print_bestmove(&ui);
    info_destroy(&ui);
    mtx_destroy(&mtxSchedule);

    return smp_nodes();
}
