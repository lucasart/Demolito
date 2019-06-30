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

const eval_t Material[NB_PIECE] = {{N, N}, {B, B}, {R, R}, {Q, Q}, {0, 0}, {OP, EP}};

eval_t pst[NB_COLOR][NB_PIECE][NB_SQUARE];

static const int Center[NB_FILE] = {-5,-2, 0, 2, 2, 0,-2,-5};

static eval_t knight(int rank, int file)
{
    const int ctr = Center[rank] + Center[file];
    return (eval_t) {10 * ctr, 51 * ctr / 16};
}

static eval_t bishop(int rank, int file)
{
    const int ctr = Center[rank] + Center[file];
    return (eval_t) {2 * ctr + 7 * (rank + file == 7 || rank - file == 0) - 18 * (rank == RANK_1), 23 * ctr / 8};
}

static eval_t rook(int rank, int file)
{
    return (eval_t) {11 * Center[file] / 4 + 15 * (rank == RANK_7), 17 * (rank == RANK_7)};
}

static eval_t queen(int rank, int file)
{
    return (eval_t) {-9 * (rank == RANK_1), 67 * (Center[rank] + Center[file]) / 16};
}

static eval_t king(int rank, int file)
{
    static const int FileWeight[NB_FILE] = {55, 70, 42, 0, 0, 42, 70, 55};
    static const int RankWeight[NB_RANK] = {26, 0, -26, -44, -58, -60, -60, -60};

    return (eval_t) {FileWeight[file] + RankWeight[rank], 107 * (Center[rank] + Center[file]) / 8};
}

static eval_t pawn(int rank, int file)
{
    eval_t e = {0, 0};

    if (file == FILE_D || file == FILE_E) {
        if (rank == RANK_3 || rank == RANK_5)
            e.op = 17;
        else if (rank == RANK_4)
            e.op = 38;
    }

    return e;
}

void eval_add(eval_t *e1, eval_t e2)
{
    e1->op += e2.op;
    e1->eg += e2.eg;
}

void eval_sub(eval_t *e1, eval_t e2)
{
    e1->op -= e2.op;
    e1->eg -= e2.eg;
}

void pst_init()
{
    typedef eval_t (*pst_fn)(int, int);
    static const pst_fn PstFn[NB_PIECE] = {&knight, &bishop, &rook, &queen, &king, &pawn};

    // Calculate PST, based on specialized functions for each piece
    for (int color = WHITE; color <= BLACK; color++)
        for (int piece = KNIGHT; piece < NB_PIECE; piece++)
            for (int square = A1; square <= H8; square++) {
                pst[color][piece][square] = Material[piece];
                eval_add(&pst[color][piece][square], (*PstFn[piece])(relative_rank_of(color, square), file_of(square)));

                if (color == BLACK) {
                    pst[color][piece][square].op *= -1;
                    pst[color][piece][square].eg *= -1;
                }
            }
}
