#pragma once
#include "types.h"

typedef uint64_t bitboard_t;

namespace bb {

void init();

/* Bitboard Accessors */

bitboard_t rank(int r);
bitboard_t file(int f);

// Leaper attacks
bitboard_t pattacks(Color c, int s);
bitboard_t nattacks(int s);
bitboard_t kattacks(int s);

// Slider attacks
bitboard_t battacks(int s, bitboard_t occ);
bitboard_t rattacks(int s, bitboard_t occ);
bitboard_t bpattacks(int s);    // pseudo-attacks (empty board)
bitboard_t rpattacks(int s);    // pseudo-attacks (empty board)

bitboard_t segment(int s1, int s2);
bitboard_t ray(int s1, int s2);

// Precalculated arrays for evaluation
bitboard_t pawn_span(Color c, int s);
bitboard_t pawn_path(Color c, int s);
bitboard_t adjacent_files(int f);
int king_distance(int s1, int s2);

/* Bit manipulation */

bool test(bitboard_t b, int s);
void clear(bitboard_t& b, int s);
void set(bitboard_t& b, int s);
bitboard_t shift(bitboard_t b, int i);

int lsb(bitboard_t b);
int msb(bitboard_t b);
int pop_lsb(bitboard_t& b);

bool several(bitboard_t b);
int count(bitboard_t b);

/* Debug */

void print(bitboard_t b);

}    // namespace bb
