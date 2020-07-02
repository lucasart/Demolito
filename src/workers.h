#pragma once
#include <setjmp.h>
#include "bitboard.h"
#include "zobrist.h"
#include "search.h"

enum {
    NB_PAWN_HASH = 16384,
    NB_REFUTATION = 1024,
    NB_FOLLOW_UP = 1024
};

typedef struct {
    uint64_t key;
    bitboard_t passed;
    eval_t eval;
} PawnEntry;

typedef struct {
    PawnEntry pawnHash[NB_PAWN_HASH];
    int16_t history[NB_COLOR][NB_SQUARE][NB_SQUARE];
    int16_t refutationHistory[NB_REFUTATION][NB_PIECE][NB_SQUARE];
    int16_t followUpHistory[NB_FOLLOW_UP][NB_PIECE][NB_SQUARE];
    ZobristStack stack;
    jmp_buf jbuf;
    uint64_t nodes;
    int eval[MAX_PLY];
} Worker;

extern Worker *Workers;
extern size_t WorkersCount;

void workers_clear(void);
void workers_prepare(size_t count);  // realloc + clear

void workers_new_search(void);
uint64_t workers_nodes(void);
