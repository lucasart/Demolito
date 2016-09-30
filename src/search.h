#pragma once
#include <atomic>
#include "position.h"
#include "zobrist.h"

namespace search {

extern int Contempt;

extern std::atomic<uint64_t> signal;
#define STOP    uint64_t(-1)

uint64_t nodes();

struct Limits {
    Limits(): depth(MAX_DEPTH), movetime(0), movestogo(0), time(0), inc(0), threads(1), nodes(0) {}
    int depth, movetime, movestogo, time, inc, threads;
    uint64_t nodes;
};

void bestmove(const Position& pos, const Limits& lim, const zobrist::GameStack& gameStack);

}
