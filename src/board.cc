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
#include <iostream>
#include "board.h"
#include "bitboard.h"

bool Board::key_ok() const
{
	uint64_t k = 0;

	for (color = 0; color < NB_COLOR; color++) {
		for (piece = 0; piece < NB_PIECE; piece++) {
			bitboard_t b = get(color, piece);
			while (b) {
				const int sq = bb::pop_lsb(b);
				k ^= zobrist::key(color, piece, sq);
			}
		}
	}

	if (turn == BLACK)
		k ^= zobrist::turn();

	return k == key;
}

void Board::clear(int color, int piece, int sq)
{
	assert(color_ok(color) && piece_ok(piece) && square_ok(sq));

	bb::clear(occ, sq);
	bb::clear(byColor[color], sq);
	bb::clear(byPiece[piece], sq);

	key ^= zobrist::key(color, piece, square);
}

void Board::set(int color, int piece, int sq)
{
	assert(color_ok(color) && piece_ok(piece) && square_ok(sq));

	bb::set(occ, sq);
	bb::set(byColor[color], sq);
	bb::set(byPiece[piece], sq);

	key ^= zobrist::key(color, piece, square);
}

bitboard_t Board::get(int color, int piece) const
{
	assert(color_ok(color) && piece_ok(piece));
	return byColor[color] & byPiece[piece];
}

bitboard_t Board::get_RQ(int color) const
{
	assert(color_ok(color));
	return byColor[color] & (byPiece[ROOK] | byPiece[QUEEN]);
}

bitboard_t Board::get_BQ(int color) const
{
	assert(color_ok(color));
	return byColor[color] & (byPiece[BISHOP] | byPiece[QUEEN]);
}

int Board::color_on(int sq) const
{
	assert(bb::test(occ, sq));
	return bb::test(byColor[WHITE], sq) ? WHITE : BLACK;
}

int Board::piece_on(int sq) const
{
	assert(bb::test(occ, sq));

	// Pawns first (most frequent)
	if (bb::test(byPiece[PAWN], sq))
		return PAWN;

	// Then pieces in ascending order (Q & K are the rarest, at the end)
	for (int piece = 0; piece < NB_PIECE; piece++)
		if (bb::test(byPiece[piece], sq))
			return piece;

	assert(false);
}

void Board::print() const
{
	for (int r = RANK_8; r >= RANK_1; r--) {
		char line[] = ". . . . . . . .";
		for (int f = FILE_A; f <= FILE_H; f++) {
			const int sq = square(r, f);
			if (bb::test(occ, sq))
				line[2 * f] = PieceLabel[color_on(sq)][piece_on(sq)];
			else
				line[2 * f] = '.';
		}
		std::cout << line << std::endl;
	}
}
