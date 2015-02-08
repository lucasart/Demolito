#pragma once
#include "types.h"

namespace bb {

void init();

/* Bitboard Accessors */

// Leaper attacks
bitboard_t pattacks(int color, int sq);
bitboard_t nattacks(int sq);
bitboard_t kattacks(int sq);

// Slider attacks
bitboard_t battacks(int sq, bitboard_t occ);
bitboard_t rattacks(int sq, bitboard_t occ);
bitboard_t bpattacks(int sq);	// pseudo-attacks (empty board)
bitboard_t rpattacks(int sq);	// pseudo-attacks (empty board)

// Segments and Rays
bitboard_t segment(int sq1, int sq2);
bitboard_t ray(int sq1, int sq2);

/* Bit manipulation */

// Individual bit operations
bool test(bitboard_t b, int sq);
void clear(bitboard_t& b, int sq);
void set(bitboard_t& b, int sq);

// Bit loop primitives
int lsb(bitboard_t b);
int msb(bitboard_t b);
int pop_lsb(bitboard_t& b);

/* Debug print */
void print(bitboard_t b);

}	// namespace bb
