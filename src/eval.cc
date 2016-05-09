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
#include "eval.h"

namespace {

bitboard_t pawn_attacks(const Position& pos, Color c)
{
    bitboard_t pawns = pos.occ(c, PAWN);
    return bb::shift(pawns & ~bb::file(FILE_A), push_inc(c) + LEFT)
           | bb::shift(pawns & ~bb::file(FILE_H), push_inc(c) + RIGHT);
}

eval_t score_mobility(int p0, int p, bitboard_t tss)
{
    const int AdjustCount[][15] = {
        {-3, -2, -1, 0, 1, 2, 3, 4, 4},
        {-4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 5, 6, 6, 7},
        {-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 6, 7, 7}
    };
    const eval_t Weight[QUEEN + 1] = {{8, 8}, {10, 10}, {4, 8}, {2, 4}};

    const int cnt = AdjustCount[p0][bb::count(tss)];
    return Weight[p] * cnt;
}

eval_t mobility(const Position& pos, Color us)
{
    const Color them = ~us;
    const bitboard_t targets = ~(pos.occ(us, PAWN) | pos.occ(us, KING) | pawn_attacks(pos, them));

    bitboard_t fss, tss, occ;
    Square from;
    eval_t result = {0, 0};

    // Knight mobility
    fss = pos.occ(us, KNIGHT);

    while (fss) {
        tss = bb::nattacks(bb::pop_lsb(fss)) & targets;
        result += score_mobility(KNIGHT, KNIGHT, tss);
    }

    // Lateral mobility
    fss = pos.occ_RQ(us);
    occ = pos.occ() ^ fss;    // RQ see through each other

    while (fss) {
        from = bb::pop_lsb(fss);
        tss = bb::rattacks(from, occ) & targets;
        result += score_mobility(ROOK, pos.piece_on(from), tss);
    }

    // Diagonal mobility
    fss = pos.occ_BQ(us);
    occ = pos.occ() ^ fss;    // BQ see through each other

    while (fss) {
        from = bb::pop_lsb(fss);
        tss = bb::battacks(from, occ) & targets;
        result += score_mobility(BISHOP, pos.piece_on(from), tss);
    }

    return result;
}

eval_t bishop_pair(const Position& pos, Color us)
{
    // FIXME: verify that both B are indeed on different color squares
    return bb::several(pos.occ(us, BISHOP)) ? eval_t {80, 100} :
           eval_t {0, 0};
}

int blend(const Position& pos, eval_t e)
{
    const int full = 4 * (N + B + R) + 2 * Q;
    const int total = pos.piece_material().eg();
    return e.op() * total / full + e.eg() * (full - total) / full;
}

}    // namespace

int evaluate(const Position& pos)
{
    assert(!pos.checkers());
    eval_t e[NB_COLOR] = {pos.pst(), {0, 0}};

    for (Color c = WHITE; c <= BLACK; ++c) {
        e[c] += bishop_pair(pos, c);
        e[c] += mobility(pos, c);
    }

    const Color us = pos.turn(), them = ~us;
    return blend(pos, e[us] - e[them]);
}
