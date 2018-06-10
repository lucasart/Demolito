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
#include <assert.h>
#include <stdlib.h>
#include "types.h"

int64_t dbgCnt[2] = {0, 0};

// Color, Piece

int opposite(int c)
{
    BOUNDS(c, NB_COLOR);
    return c ^ BLACK;
}

const char *PieceLabel[NB_COLOR] = {"NBRQKP.", "nbrqkp."};

// Rank, File

int rank_of(int s)
{
    BOUNDS(s, NB_SQUARE);
    return s / NB_FILE;
}

int file_of(int s)
{
    BOUNDS(s, NB_SQUARE);
    return s % NB_FILE;
}

int relative_rank(int c, int r)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(r, NB_RANK);
    return r ^ (7 * c);
}

int relative_rank_of(int c, int s)
{
    BOUNDS(s, NB_SQUARE);
    return relative_rank(c, rank_of(s));
}

// Square

int square(int r, int f)
{
    BOUNDS(r, NB_RANK);
    BOUNDS(f, NB_FILE);
    return NB_FILE * r + f;
}

void square_to_string(int s, char *str)
{
    BOUNDS(s, NB_SQUARE + 1);

    if (s == NB_SQUARE)
        *str++ = '-';
    else {
        *str++ = file_of(s) + 'a';
        *str++ = rank_of(s) + '1';
    }

    *str = '\0';
}

int string_to_square(const char *str)
{
    return *str != '-'
           ? square(str[1] - '1', str[0] - 'a')
           : NB_SQUARE;
}

int push_inc(int c)
{
    BOUNDS(c, NB_COLOR);
    return c == WHITE ? UP : DOWN;
}

// Eval

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

bool is_mate_score(int score)
{
    assert(abs(score) < MATE);
    return abs(score) >= MATE - MAX_PLY;
}

int mated_in(int ply)
{
    return ply - MATE;
}

int mate_in(int ply)
{
    return MATE - ply;
}
