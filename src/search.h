#pragma once
#include <atomic>
#include "position.h"

namespace search {

extern std::atomic<uint64_t> nodes;

struct Limits {
	int depth, threads;
	uint64_t nodes;
};

Move bestmove(const Position& pos, const Limits& lim);

}
