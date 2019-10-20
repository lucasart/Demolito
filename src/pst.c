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
    static const eval_t bpst[NB_RANK][NB_FILE/2] = {
        {{-7,-5}, {-5, 0}, {-7, 2}, {-1,-1}},
        {{-4,-1}, { 5,-1}, { 2, 2}, {-6, 3}},
        {{ 0,-2}, { 0, 1}, { 1,-4}, { 5, 2}},
        {{ 6,-2}, { 2, 3}, { 2,-4}, {-1, 5}},
        {{ 0,-1}, { 1, 0}, { 1,-1}, { 0, 3}},
        {{-2,-3}, { 0, 1}, { 0, 1}, {-4, 2}},
        {{-2,-3}, { 0,-1}, { 2,-2}, { 1, 0}},
        {{-3, 4}, {-3, 2}, { 1, 5}, { 2,-3}}
    };

    return bpst[rank][file > FILE_D ? FILE_H - file : file];
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
    static const eval_t kpst[NB_RANK][NB_FILE/2] = {
        {{66,-143}, {86,-103}, { 57,-58}, { 27,-41}},
        {{53, -88}, {68, -48}, { 37,-21}, {  0,  0}},
        {{26, -71}, {42, -24}, { 11,  3}, {-35, 28}},
        {{15, -29}, {29,  -3}, { -3, 30}, {-38, 41}},
        {{ 1, -39}, {12,   1}, {-19, 25}, {-54, 57}},
        {{-6, -65}, {12, -29}, {-19,  0}, {-62, 24}},
        {{-7, -80}, {10, -47}, {-14,-32}, {-59, -1}},
        {{-5,-131}, { 6,-102}, {-18,-64}, {-53,-33}}
    };

    return kpst[rank][file > FILE_D ? FILE_H - file : file];
}

static eval_t pawn(int rank, int file)
{
    static const eval_t ppst[NB_RANK][NB_FILE/2] = {
        {{ 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}},
        {{-2,-1}, { 5, 3}, { 0, 4}, {-3, 3}},
        {{-3,-2}, { 1, 1}, {-2, 1}, {15, 1}},
        {{-3, 1}, { 2,-2}, { 1, 2}, {34,-2}},
        {{ 2, 3}, {-1, 3}, { 0, 4}, {17,-2}},
        {{-3, 4}, { 4,-2}, {-3, 1}, {-2, 0}},
        {{-2,-1}, { 0,-1}, {-1, 0}, {-1, 1}},
        {{ 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}}

    };

    return ppst[rank][file > FILE_D ? FILE_H - file : file];
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
