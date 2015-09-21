#pragma once
#include "position.h"

namespace Search {

extern uint64_t nodes;

struct Limits {
	int depth;
};

Move bestmove(const Position& pos, const Limits& lim);

}
