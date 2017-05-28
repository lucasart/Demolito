#pragma once
#include "types.h"
#include "zobrist.h"

typedef struct {
    uint64_t key;
    eval_t eval;
} PawnEntry;

enum {
    NB_PAWN_ENTRY = 0x4000,
    NB_REFUTATION = 2 * 64 * 64
};

typedef struct {
    PawnEntry pawnHash[0x4000];
    int history[NB_COLOR][NB_SQUARE][NB_SQUARE];
    move_t refutation[NB_REFUTATION];
    Stack stack;
    int64_t nodes;
    int depth;
    int id;
} Worker;

extern thread_local Worker *thisWorker;
extern Worker *Workers;
extern int WorkersCount;

void smp_init();
void smp_resize(int count);
void smp_destroy();

void smp_new_search();
void smp_new_game();
int64_t smp_nodes();
