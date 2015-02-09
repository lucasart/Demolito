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
#include "zobrist.h"
#include "prng.h"
#include "bitboard.h"

namespace {

uint64_t Zobrist[NB_COLOR][NB_PIECE][NB_SQUARE];
uint64_t ZobristCastling[NB_SQUARE];
uint64_t ZobristEp[NB_SQUARE+1];
uint64_t ZobristTurn;

}	// namespace

namespace zobrist {

void init()
{
	PRNG prng;

	for (int color = 0; color < NB_COLOR; color++)
		for (int piece = 0; piece < NB_PIECE; piece++)
			for (int sq = 0; sq < NB_SQUARE; sq++)
				Zobrist[color][piece][sq] = prng.rand();

	for (int sq = 0; sq < NB_SQUARE; sq++)
		ZobristCastling[sq] = prng.rand();

	for (int sq = 0; sq <= NB_SQUARE; sq++)
		ZobristEp[sq] = prng.rand();

	ZobristTurn = prng.rand();
}

uint64_t key(int color, int piece, int sq)
{
	assert(color_ok(color) && piece_ok(piece) && square_ok(sq));
	return Zobrist[color][piece][sq];
}

uint64_t castling(bitboard_t castlable_rooks)
{
	assert(bb::count(castlable_rooks & bb::rank_bb(RANK_1)) <= 2);
	assert(bb::count(castlable_rooks & bb::rank_bb(RANK_8)) <= 2);
	assert(!(castlable_rooks & ~bb::rank_bb(RANK_1) & ~bb::rank_bb(RANK_8)));

	bitboard_t result = 0;
	while (castlable_rooks)
		result ^= ZobristCastling[bb::pop_lsb(castlable_rooks)];

	return result;
}

uint64_t ep(int sq)
{
	assert(square_ok(sq) || sq == NB_SQUARE);
	return ZobristEp[sq];
}

uint64_t turn()
{
	return ZobristTurn;
}

}	// namespace zobrist
