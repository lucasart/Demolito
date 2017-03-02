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
#include <string.h>  // memset()
#include "sort.h"

#define HISTORY_MAX 2000

namespace search {

thread_local History H;

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

        it = gen_piece_moves(pos, it, pieceTargets);
        it = gen_pawn_moves(pos, it, pawnTargets, depth > 0);

        if (depth > 0)
            it = gen_castling_moves(pos, it);
    }

    s->cnt = it - s->moves;
}

void sort_score(Sort *s, const Position *pos, move_t ttMove)
{
    for (size_t i = 0; i < s->cnt; i++) {
        if (s->moves[i] == ttMove)
            s->scores[i] = +INF;
        else {
            const Move m(s->moves[i]);

            if (move_is_capture(pos, &m)) {
                const int see = move_see(pos, &m);
                s->scores[i] = see >= 0 ? see + HISTORY_MAX : see - HISTORY_MAX;
            } else
                s->scores[i] = H.table[pos->turn][m.from][m.to];
        }
    }
}

void history_update(int c, Move m, int bonus)
{
    int *t = &H.table[c][m.from][m.to];

    *t += bonus;

    if (*t > HISTORY_MAX)
        *t = HISTORY_MAX;
    else if (*t < -HISTORY_MAX)
        *t = -HISTORY_MAX;
}

Sort::Sort(const Position *pos, int depth, move_t ttMove)
{
    sort_generate(this, pos, depth);
    sort_score(this, pos, ttMove);
    idx = 0;
}

Move sort_next(Sort *s, const Position *pos, int *see)
{
    int maxScore = -INF;
    size_t maxIdx = s->idx;

    for (size_t i = s->idx; i < s->cnt; i++)
        if (s->scores[i] > maxScore) {
            maxScore = s->scores[i];
            maxIdx = i;
        }

    if (maxIdx != s->idx) {
        std::swap(s->moves[s->idx], s->moves[maxIdx]);
        std::swap(s->scores[s->idx], s->scores[maxIdx]);
    }

    const Move m(s->moves[s->idx]);
    const int score = s->scores[s->idx];

    if (move_is_capture(pos, &m)) {
        if (score >= HISTORY_MAX)
            *see = score - HISTORY_MAX;
        else {
            assert(score < -HISTORY_MAX);
            *see = score + HISTORY_MAX;
        }
    } else
        *see = move_see(pos, &m);

    return s->moves[s->idx++];
}

}    // namespace search
