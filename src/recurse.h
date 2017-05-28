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

int generic_search(const Position *pos, int ply, int depth, int alpha, int beta, move_t pv[])
{
    const int EvalMargin[] = {0, 137, 265, 396, 511};
    const int RazorMargin[] = {0, 241, 449, 516, 818};

    assert(Qsearch == (depth <= 0));
    assert(gs_back(&thisWorker->stack) == pos->key);
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
                longjmp(jbuf, ABORT_ALL);
            else if (s & (1ULL << thisWorker->id))
                longjmp(jbuf, ABORT_ONE);
        }
    }

    move_t childPv[MAX_PLY - ply];

    if (pvNode)
        pv[0] = 0;

    if (ply > 0 && (gs_repetition(&thisWorker->stack, pos->rule50)
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

    thisWorker->nodes++;

    if (ply >= MAX_PLY)
        return refinedEval;

    // Eval pruning
    if (!Qsearch && depth <= 4 && !pos->checkers && !pvNode && pos->pieceMaterial[us].eg
            && refinedEval >= beta + EvalMargin[depth])
        return refinedEval;

    // Razoring
    if (!Qsearch && depth <= 4 && !pos->checkers && !pvNode) {
        const int lbound = alpha - RazorMargin[depth];

        if (refinedEval <= lbound) {
            score = qsearch(pos, ply, 0, lbound, lbound + 1, childPv);

            if (score <= lbound)
                return score;
        }
    }

    // Null search
    if (!Qsearch && depth >= 2 && !pvNode
            && staticEval >= beta && pos->pieceMaterial[us].eg) {
        const int nextDepth = depth - (2 + depth / 3) - (refinedEval >= beta + P);
        pos_switch(&nextPos, pos);
        gs_push(&thisWorker->stack, nextPos.key);
        score = nextDepth <= 0
                ? -qsearch(&nextPos, ply + 1, nextDepth, -beta, -(beta - 1), childPv)
                : -search(&nextPos, ply + 1, nextDepth, -beta, -(beta - 1), childPv);
        gs_pop(&thisWorker->stack);

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
        if (Qsearch && !pos->checkers && staticEval + P / 2 <= alpha && see <= 0)
            continue;

        // Play move
        pos_move(&nextPos, pos, currentMove);

        // Prune losing captures in the search, near the leaves
        if (!Qsearch && depth <= 4 && see < 0 && !pvNode && !pos->checkers && !nextPos.checkers
                && !move_is_capture(pos, currentMove))
            continue;

        gs_push(&thisWorker->stack, nextPos.key);

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
                score = -qsearch(&nextPos, ply + 1, nextDepth, -beta, -alpha, childPv);
        } else {
            // Search recursion (PVS + Reduction)
            if (moveCount == 1)
                score = -search(&nextPos, ply + 1, nextDepth, -beta, -alpha, childPv);
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
                        ? -qsearch(&nextPos, ply + 1, nextDepth - reduction, -(alpha + 1), -alpha, childPv)
                        : -search(&nextPos, ply + 1, nextDepth - reduction, -(alpha + 1), -alpha, childPv);

                // Fail high: re-search zero window at full depth
                if (reduction && score > alpha)
                    score = -search(&nextPos, ply + 1, nextDepth, -(alpha + 1), -alpha, childPv);

                // Fail high at full depth for pvNode: re-search full window
                if (pvNode && alpha < score && score < beta)
                    score = -search(&nextPos, ply + 1, nextDepth, -beta, -alpha, childPv);
            }
        }

        // Undo move
        gs_pop(&thisWorker->stack);

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
                        info_update(&ui, depth, score, smp_nodes(), pv, true);
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
