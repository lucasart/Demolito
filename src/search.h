#pragma once
#include <atomic>
#include "position.h"
#include "zobrist.h"

extern Position rootPos;

extern int Threads;
extern int Contempt;

extern std::atomic<uint64_t> signal;
#define STOP    uint64_t(-1)

struct Limits {
    int depth, movetime, movestogo, time, inc;
    uint64_t nodes;
};

void search_init();
uint64_t search_go(const Limits& lim, const GameStack& gameStack);
