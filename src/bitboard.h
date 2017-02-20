#pragma once
#include "types.h"

typedef uint64_t bitboard_t;

void bb_init();

/* Bitboard Accessors */

bitboard_t bb_rank(int r);
bitboard_t bb_file(int f);

// Leaper attacks
bitboard_t bb_pattacks(int c, int s);
bitboard_t bb_nattacks(int s);
bitboard_t bb_kattacks(int s);

// Slider attacks
bitboard_t bb_battacks(int s, bitboard_t occ);
bitboard_t bb_rattacks(int s, bitboard_t occ);
bitboard_t bb_bpattacks(int s);    // pseudo-attacks (empty board)
bitboard_t bb_rpattacks(int s);    // pseudo-attacks (empty board)

bitboard_t bb_segment(int s1, int s2);
bitboard_t bb_ray(int s1, int s2);

/* Bit manipulation */

bool bb_test(bitboard_t b, int s);
void bb_clear(bitboard_t *b, int s);
void bb_set(bitboard_t *b, int s);
bitboard_t bb_shift(bitboard_t b, int i);

int bb_lsb(bitboard_t b);
int bb_msb(bitboard_t b);
int bb_pop_lsb(bitboard_t *b);

bool bb_several(bitboard_t b);
int bb_count(bitboard_t b);

/* Debug */

void bb_print(bitboard_t b);
