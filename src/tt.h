#pragma once
#include <vector>
#include "move.h"

namespace tt {

enum {LBOUND, EXACT, UBOUND};

struct Packed {
	uint64_t key;
	int16_t score, eval, em;
	int8_t depth, bound;

	Packed() = default;
	Packed(uint64_t v) { key = v; }	// Used to zero initialize
};

// Adjust mate scores to plies from current position, instead of plies from root
int score_to_tt(int score, int ply);
int score_from_tt(int ttScore, int ply);

void clear();
void write(const Packed& p);

extern std::vector<Packed> table;

}	// namespace tt
