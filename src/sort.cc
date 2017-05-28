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
#include "move.h"
#include "position.h"
#include "smp.h"
#include "sort.h"

enum {
    HISTORY_MAX = MAX_DEPTH * MAX_DEPTH,
    SEPARATION = HISTORY_MAX + 2
};

void sort_generate(Sort *s, const Position *pos, int depth)
{
    move_t *it = s->moves;

    if (pos->checkers)
        it = gen_check_escapes(pos, it, depth > 0);
    else {
        const int us = pos->turn;
        const bitboard_t pieceTargets = depth > 0 ? ~pos->byColor[us] : pos->byColor[opposite(us)];
        const bitboard_t pawnTargets = pieceTargets | pos_ep_square_bb(pos) | bb_rank(relative_rank(us,
                                       RANK_8));

        it = gen_piece_moves(pos, it, pieceTargets, true);
        it = gen_pawn_moves(pos, it, pawnTargets, depth > 0);

        if (depth > 0)
            it = gen_castling_moves(pos, it);
    }

    s->cnt = it - s->moves;
}

void sort_score(Sort *s, const Position *pos, move_t ttMove)
{
    const uint64_t moveKey = stack_move_key(&thisWorker->stack);

    for (size_t i = 0; i < s->cnt; i++) {
        if (s->moves[i] == ttMove)
            s->scores[i] = +INF;
        else {
            if (move_is_capture(pos, s->moves[i])) {
                const int see = move_see(pos, s->moves[i]);
                s->scores[i] = see >= 0 ? see + SEPARATION : see - SEPARATION;
            } else
                s->scores[i] = s->moves[i] == thisWorker->refutation[moveKey & (NB_REFUTATION - 1)]
                               ? HISTORY_MAX + 1
                               : thisWorker->history[pos->turn][move_from(s->moves[i])][move_to(s->moves[i])];
        }
    }
}

void history_update(int c, move_t m, int bonus)
{
    const int from = move_from(m), to = move_to(m);
    int *t = &thisWorker->history[c][from][to];

    *t += bonus;

    if (*t > HISTORY_MAX)
        *t = HISTORY_MAX;
    else if (*t < -HISTORY_MAX)
        *t = -HISTORY_MAX;
}

void sort_init(Sort *s, const Position *pos, int depth, move_t ttMove)
{
    sort_generate(s, pos, depth);
    sort_score(s, pos, ttMove);
    s->idx = 0;
}

move_t sort_next(Sort *s, const Position *pos, int *see)
{
    int maxScore = -INF;
    size_t maxIdx = s->idx;

    for (size_t i = s->idx; i < s->cnt; i++)
        if (s->scores[i] > maxScore) {
            maxScore = s->scores[i];
            maxIdx = i;
        }

#define swap(x, y) do { typeof(x) tmp = x; x = y; y = tmp; } while (0);

    if (maxIdx != s->idx) {
        swap(s->moves[s->idx], s->moves[maxIdx]);
        swap(s->scores[s->idx], s->scores[maxIdx]);
    }

#undef swap

    const int score = s->scores[s->idx];
    const move_t m = s->moves[s->idx];

    if (move_is_capture(pos, m)) {
        if (score >= SEPARATION)
            *see = score - SEPARATION;
        else {
            assert(score < -SEPARATION);
            *see = score + SEPARATION;
        }
    } else
        *see = move_see(pos, m);

    return s->moves[s->idx++];
}
