#pragma once
#include <atomic>
#include "position.h"

namespace search {

extern std::atomic<uint64_t> nodeCount;
extern std::atomic<uint64_t> signal;
#define STOP	uint64_t(-1)

struct Limits {
	Limits(): depth(MAX_DEPTH), movetime(0), threads(1), nodes(0) {}
	int depth, movetime, threads;
	uint64_t nodes;
};

void bestmove(const Position& pos, const Limits& lim);

}
