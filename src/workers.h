#pragma once
#include <setjmp.h>
#include "bitboard.h"
#include "stack.h"

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
    int16_t followUpHistory[NB_REFUTATION][NB_PIECE][NB_SQUARE];
    Stack stack;
    jmp_buf jbuf;
    int64_t nodes;
    int depth;
    int id;
} Worker;

extern Worker *Workers;
extern int WorkersCount;

void workers_clear();
void workers_prepare(int count);  // realloc + clear
void workers_destroy();

void workers_new_search();
int64_t workers_nodes();
