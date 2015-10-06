#pragma once
#include <vector>
#include "move.h"

namespace tt {

enum {LBOUND, EXACT, UBOUND};

union Packed {
	struct {
		uint64_t key;
		int16_t score, eval, move;
		int8_t depth, bound;
	};
	struct {
		uint64_t word1, word2;
	};

	Packed(uint64_t v) { word1 = word2 = v; }	// Used to zero initialize
};

// Adjust mate scores to plies from current position, instead of plies from root
int score_to_tt(int score, int ply);
int score_from_tt(int ttScore, int ply);

bool read(uint64_t key, Packed& p);
void write(const Packed& p);

extern std::vector<Packed> table;

}	// namespace tt
