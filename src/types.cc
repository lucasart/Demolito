/*
 * Demolito, a UCI chess engine.
 * Copyright 2015 Lucas Braesch.
 *
 * Demolito is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Demolito is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include <cassert>
#include "types.h"

bool Chess960 = true;	// TODO: to be set by UCI

/* Move */

bool Move::ok() const
// Test if fsq, tsq, prom are within acceptable bounds. No (pseudo)legality checking performed here
{
	return square_ok(fsq) && square_ok(tsq)
		&& ((KNIGHT <= prom && prom <= QUEEN) || prom == NB_PIECE);
}

move_t Move::encode() const
{
	assert(ok());
	return fsq | (tsq << 6) | (prom << 12);
}

void Move::decode(move_t em)
{
	fsq = em & 077;
	tsq = (em >> 6) & 077;
	prom = em >> 12;
	assert(ok());
}

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

int square(int r, int f)
{
	assert(rank_ok(r) && file_ok(f));
	return NB_FILE * r + f;
}

std::string square_to_string(int sq)
{
	assert(square_ok(sq));
	char s[2] = { char(file_of(sq) + 'a'), char(rank_of(sq) + '1') };
	return std::string(s);
}

/* Directions */

int push_inc(int color)
{
	assert(color_ok(color));
	return color == WHITE ? +8 : -8;
}

/* Display */

const std::string PieceLabel[NB_COLOR] = {"NBRQKP.", "nbrqkp."};
