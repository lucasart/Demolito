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
#include <cstring>  // std::memset()
#include "sort.h"

namespace search {

thread_local History H;

void History::clear()
{
    std::memset(table, 0, sizeof(table));
}

int History::get(Move m) const
{
    return table[m.from][m.to];
}

void History::update(Move m, int bonus)
{
    int &t = table[m.from][m.to];

    t += bonus;

    if (t > Max)
        t = Max;
    else if (t < -Max)
        t = -Max;
}

void Selector::generate(const Position& pos, int depth)
{
    move_t *it = moves;

    if (pos.checkers())
        it = gen::check_escapes(pos, it, depth > 0);
    else {
        const Color us = pos.turn();
        const bitboard_t pieceTargets = depth > 0 ? ~pos.by_color(us) : pos.by_color(~us);
        const bitboard_t pawnTargets = pieceTargets | ep_square_bb(pos) | bb::rank(relative_rank(us,
                                       RANK_8));

        it = gen::piece_moves(pos, it, pieceTargets);
        it = gen::pawn_moves(pos, it, pawnTargets, depth > 0);

        if (depth > 0)
            it = gen::castling_moves(pos, it);
    }

    cnt = it - moves;
}

void Selector::score(const Position& pos, move_t ttMove)
{
    for (size_t i = 0; i < cnt; i++) {
        if (moves[i] == ttMove)
            scores[i] = +INF;
        else {
            const Move m(moves[i]);

            if (m.is_capture(pos)) {
                const int see = m.see(pos);
                scores[i] = see >= 0 ? see + History::Max : see - History::Max;
            } else
                scores[i] = H.get(m);
        }
    }
}

Selector::Selector(const Position& pos, int depth, move_t ttMove)
{
    generate(pos, depth);
    score(pos, ttMove);
    idx = 0;
}

Move Selector::select(const Position& pos, int& see)
{
    int maxScore = -INF;
    size_t swapIdx = idx;

    for (size_t i = idx; i < cnt; i++)
        if (scores[i] > maxScore) {
            maxScore = scores[i];
            swapIdx = i;
        }

    if (swapIdx != idx) {
        std::swap(moves[idx], moves[swapIdx]);
        std::swap(scores[idx], scores[swapIdx]);
    }

    const Move m(moves[idx]);

    if (m.is_capture(pos)) {
        if (scores[idx] >= History::Max)
            see = scores[idx] - History::Max;
        else {
            assert(scores[idx] < -History::Max);
            see = scores[idx] + History::Max;
        }
    } else
        see = m.see(pos);

    return moves[idx++];
}

}    // namespace search
