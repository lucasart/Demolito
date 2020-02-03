#pragma once
#include "bitboard.h"

// Piece values (1D)
enum {
    OP = 158, EP = 200, P = (OP + EP) / 2,
    N = 640, B = 640,
    R = 1046, Q = 1980
};

extern const eval_t Material[NB_PIECE];
extern eval_t pst[NB_COLOR][NB_PIECE][NB_SQUARE];

void eval_add(eval_t *e1, eval_t e2);
void eval_sub(eval_t *e1, eval_t e2);

void pst_init();
