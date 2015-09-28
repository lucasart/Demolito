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
#include "zobrist.h"
#include "bitboard.h"

namespace {

uint64_t Zobrist[NB_COLOR][NB_PIECE][NB_SQUARE];
uint64_t ZobristCastling[NB_SQUARE];
uint64_t ZobristEnPassant[NB_SQUARE+1];
uint64_t ZobristTurn;

uint64_t rotate(uint64_t x, int k)
{
	return (x << k) | (x >> (64 - k));
}

}	// namespace

namespace zobrist {

thread_local History history;

void History::push(uint64_t key)
{
	assert(idx < MAX_GAME_PLY);
	keys[idx++] = key;
}

void History::pop()
{
	assert(idx);
	idx--;
}

void PRNG::init(uint64_t seed)
{
	a = 0xf1ea5eed;
	b = c = d = seed;

	for (int i = 0; i < 20; ++i)
		rand();
}

uint64_t PRNG::rand()
{
	uint64_t e = a - rotate(b, 7);
	a = b ^ rotate(c, 13);
	b = c + rotate(d, 37);
	c = d + e;
	return d = e + a;
}

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
		ZobristEnPassant[sq] = prng.rand();

	ZobristTurn = prng.rand();
}

uint64_t key(int color, int piece, int sq)
{
	assert(color_ok(color) && piece_ok(piece) && square_ok(sq));
	return Zobrist[color][piece][sq];
}

uint64_t castling(bitboard_t castlableRooks)
{
	bitboard_t result = 0;
	while (castlableRooks)
		result ^= ZobristCastling[bb::pop_lsb(castlableRooks)];

	return result;
}

uint64_t en_passant(int sq)
{
	assert(square_ok(sq) || sq == NB_SQUARE);
	return ZobristEnPassant[sq];
}

uint64_t turn()
{
	return ZobristTurn;
}

}	// namespace zobrist
