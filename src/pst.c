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
#include "pst.h"

eval_t pst[NB_COLOR][NB_PIECE][NB_SQUARE];

static const int Center[NB_FILE] = {-5,-2, 0, 2, 2, 0,-2,-5};

static eval_t knight(int r, int f)
{
    const int c = Center[r] + Center[f];
    return (eval_t) {10 * c, 51 * c / 16};
}

static eval_t bishop(int r, int f)
{
    const int c = Center[r] + Center[f];
    return (eval_t) {2 * c + 7 * (r + f == 7 || r - f == 0) - 18 * (r == RANK_1), 23 * c / 8};
}

static eval_t rook(int r, int f)
{
    return (eval_t) {11 * Center[f] / 4 + 15 * (r == RANK_7), 17 * (r == RANK_7)};
}

static eval_t queen(int r, int f)
{
    return (eval_t) {-9 * (r == RANK_1), 67 * (Center[r] + Center[f]) / 16};
}

static eval_t king(int r, int f)
{
    const int FileWeight[NB_FILE] = {55, 70, 42, 0, 0, 42, 70, 55};
    const int RankWeight[NB_RANK] = {26, 0, -26, -44, -58, -60, -60, -60};

    return (eval_t) {FileWeight[f] + RankWeight[r], 107 * (Center[r] + Center[f]) / 8};
}

static eval_t pawn(int r, int f)
{
    eval_t e = {0, 0};

    if (f == FILE_D || f == FILE_E) {
        if (r == RANK_3 || r == RANK_5)
            e.op = 17;
        else if (r == RANK_4)
            e.op = 38;
    }

    return e;
}

void pst_init()
{
    typedef eval_t (*pst_fn)(int, int);
    const pst_fn PstFn[NB_PIECE] = {&knight, &bishop, &rook, &queen, &king, &pawn};

    // Calculate PST, based on specialized functions for each piece
    for (int c = WHITE; c <= BLACK; c++)
        for (int p = KNIGHT; p < NB_PIECE; p++)
            for (int s = A1; s <= H8; s++) {
                pst[c][p][s] = Material[p];
                eval_add(&pst[c][p][s], (*PstFn[p])(relative_rank_of(c, s), file_of(s)));

                if (c == BLACK)
                    pst[c][p][s].op *= -1, pst[c][p][s].eg *= -1;
            }
}
