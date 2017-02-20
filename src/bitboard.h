#pragma once
#include "types.h"

typedef uint64_t bitboard_t;

void bb_init();

/* Bitboard Accessors */

bitboard_t bb_rank(int r);
bitboard_t bb_file(int f);

// Attack per piece (on an empty board for sliders)
extern bitboard_t PAttacks[NB_COLOR][NB_SQUARE];
extern bitboard_t NAttacks[NB_SQUARE];
extern bitboard_t KAttacks[NB_SQUARE];
extern bitboard_t BPseudoAttacks[NB_SQUARE];
extern bitboard_t RPseudoAttacks[NB_SQUARE];

// Slider attacks (on an occupied board)
bitboard_t bb_battacks(int s, bitboard_t occ);
bitboard_t bb_rattacks(int s, bitboard_t occ);

extern bitboard_t Segment[NB_SQUARE][NB_SQUARE];
extern bitboard_t Ray[NB_SQUARE][NB_SQUARE];

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
