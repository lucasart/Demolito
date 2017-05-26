#pragma once
#include <atomic>
#include "position.h"
#include "zobrist.h"

extern std::atomic<uint64_t> signal;
enum {STOP = (uint64_t)(-1)};

struct Limits {
    int depth, movestogo;
    int64_t movetime, time, inc, nodes;
};

extern Position rootPos;
extern Stack rootStack;
extern Limits lim;
extern int Contempt;

void search_init();
int64_t search_go(void *);
