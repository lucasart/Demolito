#pragma once
#include <stdatomic.h>
#include "zobrist.h"

extern atomic_uint_fast64_t Signal;
enum {STOP = (uint_fast64_t)(-1)};

typedef struct {
    int depth, movestogo;
    int64_t movetime, time, inc, nodes;
} Limits;

extern Position rootPos;
extern Stack rootStack;
extern Limits lim;
extern int Contempt;

void search_init();
int64_t search_go();
int search_wrapper(void *);  // dummy wrapper to satisfy C11 thread API
