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
#include "types.h"

typedef uint64_t bitboard_t;

extern bitboard_t Rank[NB_RANK], File[NB_FILE];
extern bitboard_t PawnAttacks[NB_COLOR][NB_SQUARE], KnightAttacks[NB_SQUARE],
    KingAttacks[NB_SQUARE];
extern bitboard_t Segment[NB_SQUARE][NB_SQUARE];
extern bitboard_t Ray[NB_SQUARE][NB_SQUARE];

bitboard_t bb_bishop_attacks(int square, bitboard_t occ);
bitboard_t bb_rook_attacks(int square, bitboard_t occ);

bool bb_test(bitboard_t b, int square);
void bb_clear(bitboard_t *b, int square);
void bb_set(bitboard_t *b, int square);
bitboard_t bb_shift(bitboard_t b, int i);

int bb_lsb(bitboard_t b);
int bb_msb(bitboard_t b);
int bb_pop_lsb(bitboard_t *b);

bool bb_several(bitboard_t b);
int bb_count(bitboard_t b);

void bb_print(bitboard_t b);
