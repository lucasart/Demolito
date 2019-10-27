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

static const eval_t pstSeed[NB_PIECE][NB_RANK][NB_FILE / 2] = {
    {
        {{-100,-31}, {-70,-22}, {-50,-15}, {-30, -9}},
        {{ -70,-22}, {-40,-12}, {-20, -6}, {  0,  0}},
        {{ -50,-15}, {-20, -6}, {  0,  0}, { 20,  6}},
        {{ -30, -9}, {  0,  0}, { 20,  6}, { 40, 12}},
        {{ -30, -9}, {  0,  0}, { 20,  6}, { 40, 12}},
        {{ -50,-15}, {-20, -6}, {  0,  0}, { 20,  6}},
        {{ -70,-22}, {-40,-12}, {-20, -6}, {  0,  0}},
        {{-100,-31}, {-70,-22}, {-50,-15}, {-30, -9}}
    },
    {
        {{-7,-5}, {-5, 0}, {-7, 2}, {-1,-1}},
        {{-4,-1}, { 5,-1}, { 2, 2}, {-6, 3}},
        {{ 0,-2}, { 0, 1}, { 1,-4}, { 5, 2}},
        {{ 6,-2}, { 2, 3}, { 2,-4}, {-1, 5}},
        {{ 0,-1}, { 1, 0}, { 1,-1}, { 0, 3}},
        {{-2,-3}, { 0, 1}, { 0, 1}, {-4, 2}},
        {{-2,-3}, { 0,-1}, { 2,-2}, { 1, 0}},
        {{-3, 4}, {-3, 2}, { 1, 5}, { 2,-3}}
    },
    {
        {{-13, 0}, {-5, 0}, { 0, 0}, { 5, 0}},
        {{-13, 0}, {-5, 0}, { 0, 0}, { 5, 0}},
        {{-13, 0}, {-5, 0}, { 0, 0}, { 5, 0}},
        {{-13, 0}, {-5, 0}, { 0, 0}, { 5, 0}},
        {{-13, 0}, {-5, 0}, { 0, 0}, { 5, 0}},
        {{-13, 0}, {-5, 0}, { 0, 0}, { 5, 0}},
        {{  2,17}, {10,17}, {15,17}, {20,17}},
        {{-13, 0}, {-5, 0}, { 0, 0}, { 5, 0}}
    },
    {
        {{-9,-41}, {-9,-29}, {-9,-20}, {-9,-12}},
        {{ 0,-29}, { 0,-16}, { 0, -8}, { 0,  0}},
        {{ 0,-20}, { 0, -8}, { 0,  0}, { 0,  8}},
        {{ 0,-12}, { 0,  0}, { 0,  8}, { 0, 16}},
        {{ 0,-12}, { 0,  0}, { 0,  8}, { 0, 16}},
        {{ 0,-20}, { 0, -8}, { 0,  0}, { 0,  8}},
        {{ 0,-29}, { 0,-16}, { 0, -8}, { 0,  0}},
        {{ 0,-41}, { 0,-29}, { 0,-20}, { 0,-12}}
    },
    {
        {{66,-143}, {86,-103}, { 57,-58}, { 27,-41}},
        {{53, -88}, {68, -48}, { 37,-21}, {  0,  0}},
        {{26, -71}, {42, -24}, { 11,  3}, {-35, 28}},
        {{15, -29}, {29,  -3}, { -3, 30}, {-38, 41}},
        {{ 1, -39}, {12,   1}, {-19, 25}, {-54, 57}},
        {{-6, -65}, {12, -29}, {-19,  0}, {-62, 24}},
        {{-7, -80}, {10, -47}, {-14,-32}, {-59, -1}},
        {{-5,-131}, { 6,-102}, {-18,-64}, {-53,-33}}
    },
    {
        {{ 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}},
        {{-2,-1}, { 5, 3}, { 0, 4}, {-3, 3}},
        {{-3,-2}, { 1, 1}, {-2, 1}, {15, 1}},
        {{-3, 1}, { 2,-2}, { 1, 2}, {34,-2}},
        {{ 2, 3}, {-1, 3}, { 0, 4}, {17,-2}},
        {{-3, 4}, { 4,-2}, {-3, 1}, {-2, 0}},
        {{-2,-1}, { 0,-1}, {-1, 0}, {-1, 1}},
        {{ 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}}
    }
};

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
    for (int color = WHITE; color <= BLACK; color++)
        for (int piece = KNIGHT; piece < NB_PIECE; piece++)
            for (int square = A1; square <= H8; square++) {
                const int file = file_of(square), file4 = file > FILE_D ? FILE_H - file : file;

                pst[color][piece][square] = Material[piece];
                eval_add(&pst[color][piece][square], pstSeed[piece][relative_rank_of(color, square)][file4]);

                if (color == BLACK) {
                    pst[color][piece][square].op *= -1;
                    pst[color][piece][square].eg *= -1;
                }
            }
}
