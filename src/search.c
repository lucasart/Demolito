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
#include <math.h>
#include <stdlib.h>
#include "eval.h"
#include "htable.h"
#include "position.h"
#include "search.h"
#include "sort.h"
#include "uci.h"
#include "workers.h"

Position rootPos;
ZobristStack rootStack;
Limits lim;

// Protect thread scheduling decisions
static mtx_t mtxSchedule;

atomic_bool Stop;  // Stop signal raised by timer or master thread, and observed by workers

int Contempt = 10;

int draw_score(int ply)
{
    return (ply & 1 ? Contempt : -Contempt) * 2;
}

int Reduction[MAX_DEPTH + 1][MAX_MOVES];

void search_init()
{
    for (int d = 1; d <= MAX_DEPTH; d++)
        for (int cnt = 1; cnt < MAX_MOVES; cnt++)
            Reduction[d][cnt] = 0.4 * log(min(d, 31)) + 1.057 * log(min(cnt, 31));
}

static const int Tempo = 17;

static int qsearch(Worker *worker, const Position *pos, int ply, int depth, int alpha, int beta,
    bool pvNode, move_t pv[])
{
    assert(depth <= 0);
    assert(zobrist_back(&worker->stack) == pos->key);
    assert(-MATE <= alpha && alpha < beta && beta <= MATE);
    assert(pvNode || (alpha+1 == beta));

    const int oldAlpha = alpha;
    int bestScore = -MATE;
    move_t bestMove = 0;
    int score;
    Position nextPos;

    // Allocate PV for the child node
    move_t childPv[MAX_PLY - ply];

    // Terminate current PV
    if (pvNode)
        pv[0] = 0;

    if (ply > 0 && (zobrist_repetition(&worker->stack, pos) || pos_insufficient_material(pos)))
        return draw_score(ply);

    // HT probe
    HashEntry he;
    int refinedEval;

    if (hash_read(pos->key, &he, ply)) {
        if (!pvNode && ((he.score <= alpha && he.bound >= EXACT)
                || (he.score >= beta && he.bound <= EXACT))) {
            assert(he.depth >= depth);
            return he.score;
        }

        refinedEval = worker->eval[ply] = he.eval;

        if ((he.score > refinedEval && he.bound <= EXACT)
                || (he.score < refinedEval && he.bound >= EXACT))
            refinedEval = he.score;
    } else {
        he.data = 0;  // invalidate hash entry
        refinedEval = worker->eval[ply] = pos->checkers ? -MATE
           : zobrist_move_key(&worker->stack, 0) == ZobristTurn ? -worker->eval[ply - 1] + 2 * Tempo
           : evaluate(worker, pos) + Tempo;
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
    Sort sort;
    sort_init(worker, &sort, pos, depth, he.move);

    const bitboard_t pins = calc_pins(pos);
    int moveCount = 0;

    // Move loop
    while (sort.idx != sort.cnt && alpha < beta) {
        int see;
        const move_t currentMove = sort_next(&sort, pos, &see);

        if (!gen_is_legal(pos, pins, currentMove))
            continue;

        moveCount++;

        // Prune losing captures in the qsearch
        if (see < 0)
            continue;

        // SEE proxy tells us we're unlikely to beat alpha
        if (worker->eval[ply] + 97 <= alpha && see <= 0)
            continue;

        // Play move
        pos_move(&nextPos, pos, currentMove);
        hash_prefetch(nextPos.key);
        zobrist_push(&worker->stack, nextPos.key);

        const int nextDepth = depth - 1;

        // Recursion (plain alpha/beta)
        if (depth <= MIN_DEPTH && !pos->checkers) {
            score = worker->eval[ply] + see;  // guard against QSearch explosion

            if (pvNode)
                childPv[0] = 0;
        } else
            score = -qsearch(worker, &nextPos, ply + 1, nextDepth, -beta, -alpha, pvNode, childPv);

        // Undo move
        zobrist_pop(&worker->stack);

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

    // No legal check evasion: we're mated
    if (pos->checkers && !moveCount)
        return mated_in(ply);

    // Return worst possible score when all moves are pruned
    if (bestScore <= -MATE) {
        assert(bestScore == -MATE);
        return max(alpha, mated_in(ply + 1));
    }

    // HT write
    he.bound = bestScore <= oldAlpha ? UBOUND : bestScore >= beta ? LBOUND : EXACT;
    he.score = bestScore;
    he.eval = pos->checkers ? -MATE : worker->eval[ply];
    he.depth = 0;
    he.move = bestMove;
    hash_write(pos->key, &he, ply);

    return bestScore;
}

static int search(Worker *worker, const Position *pos, int ply, int depth, int alpha, int beta,
    move_t pv[], move_t singularMove)
{
    static const int EvalMargin[] = {0, 130, 264, 410, 510, 672, 840};
    static const int RazorMargin[] = {0, 229, 438, 495, 878, 1094};
    static const int SEEMargin[2][6] = {
        {0, 0, 0, 0, -179, -358},  // quiet
        {0, -33, -132, -297, -528, -825}  // capture
    };

    assert(depth > 0);
    assert(zobrist_back(&worker->stack) == pos->key);
    assert(-MATE <= alpha && alpha < beta && beta <= MATE);

    const bool pvNode = beta > alpha + 1;
    const int oldAlpha = alpha;
    const int us = pos->turn;
    int bestScore = -MATE;
    move_t bestMove = 0;
    int score;
    Position nextPos;

    if (atomic_load_explicit(&Stop, memory_order_relaxed))
        longjmp(worker->jbuf, 1);

    // Allocate PV for the child node, and terminate current PV
    move_t childPv[MAX_PLY - ply];
    pv[0] = 0;

    if (ply > 0 && (zobrist_repetition(&worker->stack, pos) || pos_insufficient_material(pos)))
        return draw_score(ply);

    // HT probe
    HashEntry he;
    int refinedEval;
    const uint64_t key = pos->key ^ singularMove;

    if (hash_read(key, &he, ply)) {
        if (he.depth >= depth && !pvNode && ((he.score <= alpha && he.bound >= EXACT)
                || (he.score >= beta && he.bound <= EXACT)))
            return he.score;

        refinedEval = worker->eval[ply] = he.eval;

        if ((he.score > refinedEval && he.bound <= EXACT)
                || (he.score < refinedEval && he.bound >= EXACT))
            refinedEval = he.score;
    } else {
        he.data = 0;  // invalidate hash entry
        refinedEval = worker->eval[ply] = pos->checkers ? -MATE
           : zobrist_move_key(&worker->stack, 0) == ZobristTurn ? -worker->eval[ply - 1] + 2 * Tempo
           : evaluate(worker, pos) + Tempo;
    }

    // At Root, ensure that the last best move is searched first. This is not guaranteed,
    // as the HT entry could have got overriden by other search threads.
    if (ply == 0 && info_last_depth(&ui) > 0)
        he.move = info_best(&ui);

    worker->nodes++;

    if (ply >= MAX_PLY)
        return refinedEval;

    // Eval pruning
    if (depth <= 6 && !pos->checkers && !pvNode && pos->pieceMaterial[us]
            && refinedEval >= beta + EvalMargin[depth])
        return refinedEval;

    // Razoring
    if (depth <= 5 && !pos->checkers && !singularMove && !pvNode) {
        const int lbound = alpha - RazorMargin[depth];

        if (refinedEval <= lbound) {
            if (depth <= 2)
                return qsearch(worker, pos, ply, 0, alpha, alpha + 1, false, childPv);

            score = qsearch(worker, pos, ply, 0, lbound, lbound + 1, false, childPv);

            if (score <= lbound)
                return score;
        }
    }

    // Null search
    if (depth >= 2 && !pvNode && !pos->checkers
            && worker->eval[ply] >= beta && pos->pieceMaterial[us]) {
        // Normallw worker->eval[ply] >= beta excludes the in check case (eval is -MATE). But with
        // HT collisions or races, HT data can't be trusted. Doing a null move in check crashes for
        // obvious reasons, so it must be explicitely prevented.
        const int nextDepth = depth - (3 + depth / 4) - (refinedEval >= beta + 167);

        pos_switch(&nextPos, pos);
        zobrist_push(&worker->stack, nextPos.key);

        score = nextDepth <= 0
            ? -qsearch(worker, &nextPos, ply + 1, nextDepth, -beta, -(beta - 1), false, childPv)
            : -search(worker, &nextPos, ply + 1, nextDepth, -beta, -(beta - 1), childPv, 0);

        zobrist_pop(&worker->stack);

        if (score >= beta)
            return score >= mate_in(MAX_PLY) ? beta : score;
    }

    // Generate and score moves
    Sort sort;
    sort_init(worker, &sort, pos, depth, he.move);

    const bitboard_t pins = calc_pins(pos);
    int moveCount = 0, lmrCount = 0;
    move_t quietSearched[MAX_MOVES];
    int quietSearchedCnt = 0;

    // Move loop
    while (sort.idx != sort.cnt && alpha < beta) {
        int see;
        const move_t currentMove = sort_next(&sort, pos, &see);

        if (!gen_is_legal(pos, pins, currentMove) || currentMove == singularMove)
            continue;

        moveCount++;

        // Limit the number of moves search to prove singularity
        if (singularMove && moveCount >= 5)
            break;

        const bool capture = pos_move_is_capture(pos, currentMove);

        if (!capture)
            quietSearched[quietSearchedCnt++] = currentMove;

        // Play move
        pos_move(&nextPos, pos, currentMove);

        const bool improving = ply < 2 || worker->eval[ply] > worker->eval[ply - 2];

        // Prune bad or late moves near the leaves
        if (depth <= 5 && !pvNode && !nextPos.checkers) {
            // SEE pruning
            if (see < SEEMargin[capture][depth])
               continue;

            // Late Move Pruning
            if (!capture && depth <= 4 && moveCount >= 3 * depth + 2 * improving)
                break;
        }

        hash_prefetch(nextPos.key);

        // Search extension
        int ext = 0;

        if (currentMove == he.move && ply > 0 && depth >= 5 && he.bound <= EXACT && he.depth >= depth - 4) {
            // Singular Extension Search
            const int lbound = he.score - 2 * depth;

            if (abs(lbound) < MATE) {
                score = search(worker, pos, ply, depth - 4, lbound, lbound + 1, childPv, currentMove);
                ext = score <= lbound;
            }
        } else
            // Check extension
            ext = see >= 0 && nextPos.checkers;

        zobrist_push(&worker->stack, nextPos.key);

        const int nextDepth = depth - 1 + ext;

        // Recursion
        if (nextDepth <= 0)
            score = -qsearch(worker, &nextPos, ply + 1, nextDepth, -beta, -alpha, pvNode, childPv);
        else {
            // Search recursion (PVS + Reduction)
            if (moveCount == 1)
                score = -search(worker, &nextPos, ply + 1, nextDepth, -beta, -alpha, childPv, 0);
            else {
                int reduction = see < 0 || !capture;

                if (!capture) {
                    lmrCount++;
                    assert(1 <= nextDepth && nextDepth <= MAX_DEPTH);
                    assert(1 <= lmrCount && lmrCount <= MAX_MOVES);
                    reduction = Reduction[nextDepth][lmrCount] + !improving;

                    if (sort.scores[sort.idx - 1] >= 1024)
                        reduction = max(0, reduction - 1);
                }

                // Reduced depth, zero window
                score = nextDepth - reduction <= 0
                    ? -qsearch(worker, &nextPos, ply + 1, nextDepth - reduction, -(alpha + 1), -alpha, false, childPv)
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
        zobrist_pop(&worker->stack);

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

                    // Best move has changed since last completed iteration. Update the best move and
                    // PV immediately, because we may not have time to finish this iteration.
                    if (ply == 0 && moveCount > 1 && depth > 1)
                        info_update(&ui, depth, score, workers_nodes(), pv, true);
                }
            }
        }
    }

    // No legal move: mated or stalemated
    if (!moveCount)
        return singularMove ? alpha : pos->checkers ? mated_in(ply) : draw_score(ply);

    // Return worst possible score when all moves are pruned
    if (bestScore <= -MATE) {
        assert(bestScore == -MATE);
        return max(alpha, mated_in(ply + 1));
    }

    // Update move sorting statistics
    if (alpha > oldAlpha && !singularMove && !pos_move_is_capture(pos, bestMove)) {
        const size_t rhIdx = zobrist_move_key(&worker->stack, 0) % NB_REFUTATION;
        const size_t fuhIdx = zobrist_move_key(&worker->stack, 1) % NB_FOLLOW_UP;

        for (int i = 0; i < quietSearchedCnt; i++) {
            const int bonus = quietSearched[i] == bestMove ? depth * depth : -1 - depth * depth / 2;
            const int from = move_from(quietSearched[i]), to = move_to(quietSearched[i]);

            history_update(&worker->history[us][from][to], bonus);
            history_update(&worker->refutationHistory[rhIdx][pos_piece_on(pos, from)][to], bonus);
            history_update(&worker->followUpHistory[fuhIdx][pos_piece_on(pos, from)][to], bonus);
        }
    }

    // HT write
    he.bound = bestScore <= oldAlpha ? UBOUND : bestScore >= beta ? LBOUND : EXACT;
    he.score = bestScore;
    he.eval = pos->checkers ? -MATE : worker->eval[ply];
    he.depth = depth;
    he.move = bestMove;
    hash_write(key, &he, ply);

    return bestScore;
}

static int aspirate(Worker *worker, int depth, move_t pv[], int score)
{
    assert(depth > 0);

    if (depth == 1)
        return search(worker, &rootPos, 0, depth, -MATE, MATE, pv, 0);

    int delta = 15;
    int alpha = max(score - delta, -MATE);
    int beta = min(score + delta, MATE);

    for ( ; ; delta *= 1.876) {
        score = search(worker, &rootPos, 0, depth, alpha, beta, pv, 0);

        if (score <= alpha) {
            beta = (alpha + beta) / 2;
            alpha = max(alpha - delta, -MATE);
        } else if (score >= beta) {
            alpha = (alpha + beta) / 2;
            beta = min(beta + delta, MATE);
        } else
            return score;
    }
}

static void iterate(Worker *worker)
{
    move_t pv[MAX_PLY + 1];
    int volatile score = 0;

    for (volatile int depth = 1; depth <= lim.depth; depth++) {
        // If half of the threads are searching >= depth, then move to the next depth.
        // Special cases where this does not apply:
        // depth == 1: we want all threads to finish depth == 1 asap.
        // depth == lim.depth: there is no next depth.
        mtx_lock(&mtxSchedule);

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

        if (!setjmp(worker->jbuf))
            score = aspirate(worker, depth, pv, score);
        else {
            worker->stack.idx = rootStack.idx;  // Restore stack position
            break;
        }

        info_update(&ui, depth, score, workers_nodes(), pv, false);
    }

    // Max depth completed by current thread. All threads should stop. Unless we are in infinite
    // or pondering, in which case workers wait here, and the timer loop continues until stopped.
    if (!lim.infinite)
        Stop = true;
}

int mated_in(int ply)
{
    return ply - MATE;
}

int mate_in(int ply)
{
    return MATE - ply;
}

bool is_mate_score(int score)
{
    assert(abs(score) < MATE);
    return abs(score) >= MATE - MAX_PLY;
}

int64_t search_go()
{
    int64_t start = system_msec();

    info_create(&ui);
    mtx_init(&mtxSchedule, mtx_plain);
    Stop = false;

    hashDate++;
    pthread_t threads[WorkersCount];
    workers_new_search();

    int minTime = 0, maxTime = 0;  // Silence bogus gcc warning (maybe uninitialized)

    if (!lim.movetime && (lim.time || lim.inc)) {
        const int movesToGo = lim.movestogo ? 0.5 + pow(lim.movestogo, 0.9) : 26;
        const int remaining = (movesToGo - 1) * lim.inc + lim.time;

        minTime = min(0.57 * remaining / movesToGo, lim.time - uciTimeBuffer);
        maxTime = min(2.21 * remaining / movesToGo, lim.time - uciTimeBuffer);
    }

    for (int i = 0; i < WorkersCount; i++)
        // Start searching thread
        pthread_create(&threads[i], NULL, (void*(*)(void*))iterate, &Workers[i]);

    do {
        sleep_msec(5);

        // Check for search termination conditions, but only after depth 1 has been
        // completed, to make sure we do not return an illegal move.
        if (!lim.infinite && info_last_depth(&ui) > 0) {
            if ((lim.movetime && system_msec() - start >= lim.movetime - uciTimeBuffer)
                    || (lim.nodes && workers_nodes() >= lim.nodes))
                atomic_store_explicit(&Stop, true, memory_order_release);
            else if (lim.time || lim.inc) {
                const double x = 1 / (1 + exp(-info_variability(&ui)));
                const int64_t t = x * maxTime + (1 - x) * minTime;

                if (system_msec() - start >= t)
                    atomic_store_explicit(&Stop, true, memory_order_release);
            }
        }
    } while (!atomic_load_explicit(&Stop, memory_order_acquire));

    for (int i = 0; i < WorkersCount; i++)
        pthread_join(threads[i], NULL);

    info_print_bestmove(&ui);
    info_destroy(&ui);
    mtx_destroy(&mtxSchedule);

    return workers_nodes();
}
