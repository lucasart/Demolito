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
#pragma once
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#define BOUNDS(v, ub) assert((unsigned)(v) < (ub))

#define min(x, y) ({ \
    typeof(x) _x = (x), _y = (y); \
    _x < _y ? _x : _y; \
})

#define max(x, y) ({ \
    typeof(x) _x = (x), _y = (y); \
    _x > _y ? _x : _y; \
})

enum {RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, NB_RANK};
enum {FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, NB_FILE};

enum {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NB_SQUARE
};

enum {UP = 8, DOWN = -8, LEFT = -1, RIGHT = 1};

enum {WHITE, BLACK, NB_COLOR};
enum {KNIGHT, BISHOP, ROOK, QUEEN, KING, PAWN, NB_PIECE};

// Evaluation in 2D (opening, endgame)
typedef struct {
    int op, eg;
} eval_t;

// 16-bit move encoding: from:6, to:6, prom: 4 (NB_PIECE if none)
typedef uint16_t move_t;

extern int64_t dbgCnt[2];

void eval_add(eval_t *e1, eval_t e2);
void eval_sub(eval_t *e1, eval_t e2);
bool eval_eq(eval_t e1, eval_t e2);

int opposite(int color);
int push_inc(int color);

int square_from(int rank, int file);
int rank_of(int square);
int file_of(int square);

int relative_rank(int color, int rank);
int relative_rank_of(int color, int square);

int move_from(move_t m);
int move_to(move_t m);
int move_prom(move_t m);
move_t move_build(int from, int to, int prom);
