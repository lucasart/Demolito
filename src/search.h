#pragma once
#include <atomic>
#include "position.h"
#include "zobrist.h"

extern Position rootPos;

extern int Threads;
extern int Contempt;

extern std::atomic<uint64_t> signal;
enum {STOP = (uint64_t)(-1)};

struct Limits {
    int depth, movestogo;
    int64_t movetime, time, inc, nodes;
};

void search_init();
int64_t search_go(const Limits& lim, const GameStack& gameStack);
