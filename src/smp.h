#pragma once
#include <setjmp.h>
#include "bitboard.h"
#include "search.h"

enum {
    NB_PAWN_ENTRY = 16384,
    NB_REFUTATION = 1024,
    NB_FOLLOW_UP = 1024
};

void stack_clear(Stack *st);
void stack_push(Stack *st, uint64_t key);
void stack_pop(Stack *st);
uint64_t stack_back(const Stack *st);
uint64_t stack_move_key(const Stack *st, int back);
bool stack_repetition(const Stack *st, int rule50);

typedef struct {
    uint64_t key;
    bitboard_t passed;
    eval_t eval;
} PawnEntry;

typedef struct {
    PawnEntry pawnHash[NB_PAWN_ENTRY];
    int16_t history[NB_COLOR][NB_SQUARE * NB_SQUARE];
    int16_t refutationHistory[NB_REFUTATION][NB_PIECE][NB_SQUARE];
    int16_t followUpHistory[NB_REFUTATION][NB_PIECE][NB_SQUARE];
    move_t killers[MAX_DEPTH * 3 / 2];  // Conservative upper-bound (for qsearch and extensions)
    Stack stack;
    jmp_buf jbuf;
    int64_t nodes;
    int depth;
    int id;
} Worker;

extern Worker *Workers;
extern int WorkersCount;

void smp_clear();
void smp_prepare(int count);  // realloc + clear
void smp_destroy();

void smp_new_search();
int64_t smp_nodes();
