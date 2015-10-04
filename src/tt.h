#pragma once
#include <vector>
#include "move.h"

namespace tt {

enum {EXACT, LBOUND, UBOUND};

struct Packed;

struct UnPacked {
	uint64_t key;
	int depth, score, eval, bound;
	move_t em;

	Packed pack() const;
};

struct Packed {
	uint64_t word[2];

	UnPacked unpack() const;
};

extern std::vector<Packed> table;

}	// namespace tt
