/*
 * Demolito, a UCI chess engine.
 * Copyright 2015 Lucas Braesch.
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

/* Color, Piece */

bool color_ok(int c)
{
	return 0 <= c && c < NB_COLOR;
}

bool piece_ok(int p)
{
	return 0 <= p && p < NB_PIECE;
}

int opp_color(int c)
{
	assert(color_ok(c));
	return c ^ BLACK;
}

/* Rank, File, Square */

bool rank_ok(int r)
{
	return 0 <= r && r < NB_RANK;
}

bool file_ok(int f)
{
	return 0 <= f && f < NB_FILE;
}

bool square_ok(int sq)
{
	return 0 <= sq && sq < NB_SQUARE;
}

int rank_of(int sq)
{
	assert(square_ok(sq));
	return sq / NB_FILE;
}

int file_of(int sq)
{
	assert(square_ok(sq));
	return sq % NB_FILE;
}

int relative_rank(int color, int sq)
{
	assert(color_ok(color) && square_ok(sq));
	return rank_of(sq) ^ (7 * color);
}

int square(int r, int f)
{
	assert(rank_ok(r) && file_ok(f));
	return NB_FILE * r + f;
}

std::string square_to_string(int sq)
{
	assert(square_ok(sq));
	const char s[3] = {char(file_of(sq) + 'a'), char(rank_of(sq) + '1'), '\0'};
	return std::string(s);
}

int string_to_square(const std::string& s)
{
	return s == "-" ? NB_SQUARE : square(s[1] - '1', s[0] - 'a');
}

/* Directions */

int push_inc(int color)
{
	assert(color_ok(color));
	return color == WHITE ? UP : DOWN;
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
