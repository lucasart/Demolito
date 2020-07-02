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
#include <limits.h>
#include <stdlib.h>
#include "bitboard.h"
#include "position.h"
#include "search.h"
#include "sort.h"

enum {
    HISTORY_MAX = MAX_DEPTH * MAX_DEPTH,
    SEPARATION = 3 * HISTORY_MAX + 1
};

void sort_generate(Sort *sort, const Position *pos, int depth)
{
    move_t *it = sort->moves;

    if (pos->checkers)
        it = gen_check_escapes(pos, it, depth > 0);
    else {
        const int us = pos->turn;
        const bitboard_t pieceFilter = depth > 0 ? ~pos->byColor[us] : pos->byColor[opposite(us)];
        const bitboard_t pawnFilter = pieceFilter | pos_ep_square_bb(pos) | Rank[relative_rank(us,
            RANK_8)];

        it = gen_piece_moves(pos, it, pieceFilter, true);
        it = gen_pawn_moves(pos, it, pawnFilter, depth > 0);

        if (depth > 0)
            it = gen_castling_moves(pos, it);
    }

    sort->cnt = (size_t)(it - sort->moves);
}

void sort_score(Worker *worker, Sort *sort, const Position *pos, move_t ttMove)
{
    const size_t rhIdx = zobrist_move_key(&worker->stack, 0) % NB_REFUTATION;
    const size_t fuhIdx = zobrist_move_key(&worker->stack, 1) % NB_FOLLOW_UP;

    for (size_t i = 0; i < sort->cnt; i++) {
        const move_t m = sort->moves[i];

        if (m == ttMove)
            sort->scores[i] = INT_MAX;
        else {
            if (pos_move_is_capture(pos, m)) {
                const int see = pos_see(pos, m);
                sort->scores[i] = see >= 0 ? see + SEPARATION : see - SEPARATION;
            } else {
                const int from = move_from(m), to = move_to(m);
                sort->scores[i] = worker->history[pos->turn][from][to]
                    + worker->refutationHistory[rhIdx][pos_piece_on(pos, from)][to]
                    + worker->followUpHistory[fuhIdx][pos_piece_on(pos, from)][to];
            }
        }
    }
}

void history_update(int16_t *t, int bonus)
{
    // Do all calculations on 32-bit, and only convert back to 16-bits once we are certain that
    // there can be no overflow (signed int overflow is undefined in C).
    int v = *t;

    v += 32 * bonus - v * abs(bonus) / 128;
    v = min(v, HISTORY_MAX);  // cap
    v = max(v, -HISTORY_MAX);  // floor

    *t = v;
}

void sort_init(Worker *worker, Sort *sort, const Position *pos, int depth, move_t ttMove)
{
    sort_generate(sort, pos, depth);
    sort_score(worker, sort, pos, ttMove);
    sort->idx = 0;
}

move_t sort_next(Sort *sort, const Position *pos, int *see)
{
    int maxScore = INT_MIN;
    size_t maxIdx = sort->idx;

    for (size_t i = sort->idx; i < sort->cnt; i++)
        if (sort->scores[i] > maxScore) {
            maxScore = sort->scores[i];
            maxIdx = i;
        }

    #define swap(x, y) do { typeof(x) tmp = x; x = y; y = tmp; } while (0);

    if (maxIdx != sort->idx) {
        swap(sort->moves[sort->idx], sort->moves[maxIdx]);
        swap(sort->scores[sort->idx], sort->scores[maxIdx]);
    }

    #undef swap

    const int score = sort->scores[sort->idx];
    const move_t m = sort->moves[sort->idx];

    if (pos_move_is_capture(pos, m)) {
        // Deduce SEE from the sort score
        if (score >= SEPARATION)
            *see = score == INT_MAX
                ? pos_see(pos, m)  // special case: HT move is scored as INT_MAX
                : score - SEPARATION;  // Good captures are scored as SEE + SEPARATION
        else {
            assert(score < -SEPARATION);
            *see = score + SEPARATION;  // Bad captures are scored as SEE - SEPARATION
        }

        assert(*see == pos_see(pos, m));
    } else
        *see = pos_see(pos, m);

    return sort->moves[sort->idx++];
}
