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

/* Rank, File, Square */

Rank rank_of(int sq)
{
    BOUNDS(sq, NB_SQUARE);

    return Rank(sq / NB_FILE);
}

File file_of(int sq)
{
    BOUNDS(sq, NB_SQUARE);

    return File(sq % NB_FILE);
}

Rank relative_rank(Color c, int sq)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(sq, NB_SQUARE);

    return Rank(rank_of(sq) ^ (7 * c));
}

int square(Rank r, File f)
{
    BOUNDS(r, NB_RANK);
    BOUNDS(f, NB_FILE);

    return NB_FILE * r + f;
}

std::string square_to_string(int sq)
{
    BOUNDS(sq, NB_SQUARE);

    const char s[3] = {char(file_of(sq) + 'a'), char(rank_of(sq) + '1'), '\0'};

    return std::string(s);
}

int string_to_square(const std::string& s)
{
    return s != "-"
           ? square(Rank(s[1] - '1'), File(s[0] - 'a'))
           : NB_SQUARE;
}

/* Directions */

int push_inc(Color c)
{
    BOUNDS(c, NB_COLOR);

    return c == WHITE ? UP : DOWN;
}

/* Eval */

const eval_t Material[NB_PIECE] = {{N, N}, {B, B}, {R, R}, {Q, Q}, {0, 0}, {OP, EP}};

bool score_ok(int score)
{
    return std::abs(score) < MATE;
}

bool is_mate_score(int score)
{
    score_ok(score);
    return std::abs(score) >= MATE - MAX_PLY;
}

int mated_in(int ply)
{
    return ply - MATE;
}

int mate_in(int ply)
{
    return MATE - ply;
}

/* Display */

const std::string PieceLabel[NB_COLOR] = {"NBRQKP.", "nbrqkp."};
