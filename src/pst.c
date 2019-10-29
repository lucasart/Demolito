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
        {{-6, 0}, {-4, 4}, {-11, 2}, { 2,-5}},
        {{-4,-2}, { 8, 2}, {  6, 5}, {-8, 4}},
        {{-3,-4}, { 3, 2}, {  2,-3}, {11, 3}},
        {{14,-4}, { 0, 3}, { -3,-1}, { 1, 4}},
        {{-6, 2}, { 1,-2}, { -2, 1}, { 2, 3}},
        {{-5, 2}, {-4, 1}, { -5, 2}, { 1, 5}},
        {{-2,-2}, {-3,-1}, {  4,-3}, {-5, 4}},
        {{-6, 7}, {-4, 1}, {  3, 4}, { 3,-2}}
    },
    {
        {{-17, 0}, { -3, 0}, {-4, 0}, {-1,-1}},
        {{ -9,-4}, {-10,-2}, {-2,-1}, {10, 2}},
        {{-15,-2}, { -2, 1}, { 0, 4}, { 6, 2}},
        {{-13,-2}, { -6, 2}, {-3,-1}, {-5, 0}},
        {{-12, 1}, { -4, 4}, { 4, 4}, { 7, 2}},
        {{-13, 0}, { -2, 0}, { 5, 3}, {11, 4}},
        {{  3,21}, { 13,12}, {11,17}, {21,16}},
        {{-15,-1}, { -3,-2}, {-2, 2}, { 6, 0}}
    },
    {
        {{-8,-34}, {-4,-27}, {-9,-25}, {-7,-12}},
        {{-1,-38}, { 0,-16}, { 0, -9}, { 2,  2}},
        {{ 2,-21}, {-2,-15}, { 0,  4}, {-3,  8}},
        {{ 2, -8}, { 0,  1}, { 1,  7}, { 0, 15}},
        {{-1,-15}, {-1, -1}, {-5,  9}, { 2, 11}},
        {{ 4,-21}, {-3, -7}, {-5, -1}, { 2, 11}},
        {{-5,-30}, { 2,-17}, {-5,-10}, {-1,  1}},
        {{ 0,-31}, { 1,-26}, { 0,-16}, { 2,-13}}
    },
    {
        {{ 59,-138}, {91,-102}, { 59,-64}, { 26,-56}},
        {{ 54, -83}, {76, -53}, { 44,-17}, {  4, -3}},
        {{ 32, -69}, {50, -17}, {  7,  1}, {-46, 26}},
        {{ 17, -24}, {24,   2}, {  1, 23}, {-40, 43}},
        {{  2, -27}, {15,   3}, {-19, 26}, {-53, 59}},
        {{ -8, -72}, {16, -33}, {-29,  1}, {-56, 20}},
        {{-16, -75}, {17, -39}, {-12,-29}, {-53,  1}},
        {{ -4,-119}, { 3,-115}, {-21,-57}, {-55,-27}}
    },
    {
        {{ 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}},
        {{-5,-4}, { 8, 3}, { 7, 4}, {-5, 6}},
        {{-9,-4}, { 3,-1}, {-5, 0}, {11,-1}},
        {{-8, 6}, { 3, 0}, { 5,-1}, {39,-6}},
        {{ 2, 3}, {-1, 4}, { 4, 3}, {22,-1}},
        {{-1, 8}, {12, 3}, {-9,-4}, {-2, 5}},
        {{-6,-3}, { 2, 1}, {-4,-2}, { 2, 1}},
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
