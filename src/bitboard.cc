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
#include "bitboard.h"

namespace {

const int PDir[2][2] = {{1,-1},{1,1}};
const int NDir[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
const int KDir[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};

bitboard_t PAttacks[NB_COLOR][NB_SQUARE];
bitboard_t NAttacks[NB_SQUARE];
bitboard_t KAttacks[NB_SQUARE];

bitboard_t BPseudoAttacks[NB_SQUARE];
bitboard_t RPseudoAttacks[NB_SQUARE];

bitboard_t Segment[NB_SQUARE][NB_SQUARE];
bitboard_t Ray[NB_SQUARE][NB_SQUARE];

void safe_set_bit(bitboard_t& b, int r, int f)
{
	if (rank_ok(r) && file_ok(f))
		bb::set(b, square(r, f));
}

void init_non_slider_attacks()
{
	for (int sq = 0; sq < NB_SQUARE; sq++) {
		int r = rank_of(sq), f = file_of(sq);

		for (int d = 0; d < 8; d++) {
			safe_set_bit(NAttacks[sq], r + NDir[d][0], f + NDir[d][1]);
			safe_set_bit(KAttacks[sq], r + KDir[d][0], f + KDir[d][1]);
		}

		for (int d = 0; d < 2; d++) {
			safe_set_bit(PAttacks[WHITE][sq], r + PDir[d][0], f + PDir[d][1]);
			safe_set_bit(PAttacks[BLACK][sq], r - PDir[d][0], f - PDir[d][1]);
		}
	}
}

void init_rays()
{
	for (int sq1 = 0; sq1 < NB_SQUARE; sq1++) {
		int r1 = rank_of(sq1), f1 = file_of(sq1);
		for (int d = 0; d < 8; d++) {
			bitboard_t mask = 0;
			for (int r2 = r1, f2 = f1; rank_ok(r2) && file_ok(f2); r2 += KDir[d][0], f2 += KDir[d][1]) {
				int sq2 = square(r2, f2);
				bb::set(mask, sq2);
				Segment[sq1][sq2] = mask;
			}

			bitboard_t sqs = mask;
			while (sqs) {
				int sq2 = bb::pop_lsb(sqs);
				Ray[sq1][sq2] = mask;
			}
		}
	}
}

void init_slider_pseudo_attacks()
{
	for (int sq = 0; sq < NB_SQUARE; sq++) {
		BPseudoAttacks[sq] = bb::battacks(sq, 0);
		RPseudoAttacks[sq] = bb::rattacks(sq, 0);
	}
}

}	// namespace

namespace bb {

void init_slider_attacks();	// in magic.cc

void init()
{
	init_rays();
	init_non_slider_attacks();
	init_slider_attacks();
	init_slider_pseudo_attacks();
}

/* Bitboard Accessors */

bitboard_t rank(int r)
{
	assert(rank_ok(r));
	return 0xFFULL << (8 * r);
}

bitboard_t file(int f)
{
	assert(file_ok(f));
	return 0x0101010101010101ULL << f;
}

bitboard_t relative_rank(int color, int r)
{
	assert(color_ok(color) && rank_ok(r));
	return rank(color == WHITE ? r : RANK_8 - r);
}

bitboard_t pattacks(int color, int sq)
{
	assert(color_ok(color) && square_ok(sq));
	return PAttacks[color][sq];
}

bitboard_t nattacks(int sq)
{
	assert(square_ok(sq));
	return NAttacks[sq];
}

bitboard_t kattacks(int sq)
{
	assert(square_ok(sq));
	return KAttacks[sq];
}

bitboard_t bpattacks(int sq)
{
	assert(square_ok(sq));
	return BPseudoAttacks[sq];
}

bitboard_t rpattacks(int sq)
{
	assert(square_ok(sq));
	return RPseudoAttacks[sq];
}

bitboard_t piece_attacks(int color, int piece, int sq, bitboard_t occ)
{
	assert(color_ok(color) && piece_ok(piece) && square_ok(sq));

	if (piece == PAWN)
		return pattacks(color, sq);
	else if (piece == KNIGHT)
		return nattacks(sq);
	else if (piece == BISHOP)
		return battacks(sq, occ);
	else if (piece == ROOK)
		return rattacks(sq, occ);
	else if (piece == QUEEN)
		return rattacks(sq, occ) | battacks(sq, occ);
	else
		return kattacks(sq);
}

bitboard_t segment(int sq1, int sq2)
{
	assert(square_ok(sq1) && square_ok(sq2));
	return Segment[sq1][sq2];
}

bitboard_t ray(int sq1, int sq2)
{
	assert(square_ok(sq1) && square_ok(sq2));
	assert(sq1 != sq2);	// Ray[sq][sq] is undefined
	return Ray[sq1][sq2];
}

/* Bit manipulation */

bool test(bitboard_t b, int sq)
{
	assert(square_ok(sq));
	return b & (1ULL << sq);
}

void clear(bitboard_t& b, int sq)
{
	assert(square_ok(sq) && test(b, sq));
	b ^= 1ULL << sq;
}

void set(bitboard_t& b, int sq)
{
	assert(square_ok(sq) && !test(b, sq));
	b ^= 1ULL << sq;
}

int lsb(bitboard_t b)
{
	assert(b);
	return __builtin_ffsll(b) - 1;
}

int msb(bitboard_t b)
{
	assert(b);
	return 63 - __builtin_clzll(b);
}

int pop_lsb(bitboard_t& b)
{
	int sq = lsb(b);
	b &= b - 1;
	return sq;
}

bool several(bitboard_t b)
{
	return b & (b - 1);
}

int count(bitboard_t b)
{
	return __builtin_popcountll(b);
}

/* Debug print */

void print(bitboard_t b)
{
	for (int r = RANK_8; r >= RANK_1; r--) {
		char line[] = ". . . . . . . .";
		for (int f = FILE_A; f <= FILE_H; f++) {
			if (test(b, square(r, f)))
				line[2 * f] = 'X';
		}
		std::cout << line << '\n';
	}
	std::cout << std::endl;
}

}	// namespace bb
