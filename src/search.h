#pragma once
#include "position.h"

namespace search {

extern uint64_t nodes;

struct Limits {
	int depth;
};

Move bestmove(const Position& pos, const Limits& lim);

}
