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
#include "types.h"

bool Chess960 = false;

int64_t dbgCnt[2] = {0, 0};

/* int, int, int */

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

/* Directions */

int push_inc(int c)
{
    BOUNDS(c, NB_COLOR);

    return c == WHITE ? UP : DOWN;
}

/* Eval */

const eval_t Material[NB_PIECE] = {{N, N}, {B, B}, {R, R}, {Q, Q}, {0, 0}, {OP, EP}};

bool score_ok(int score)
{
    return abs(score) < MATE;
}

bool is_mate_score(int score)
{
    score_ok(score);
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

/* Misc */

int64_t elapsed_msec(const struct timespec *start)
{
    struct timespec finish;
    timespec_get(&finish, TIME_UTC);
    return (finish.tv_sec - start->tv_sec) * 1000 + (finish.tv_nsec - start->tv_nsec) / 1000000;
}

const char *PieceLabel[NB_COLOR] = {"NBRQKP.", "nbrqkp."};
