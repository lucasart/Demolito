#pragma once
#include "gen.h"

namespace sort {

enum Phase {SEARCH, QSEARCH};

class Selector {
public:
	Selector(const Position& pos, Phase ph);
	Move select(const Position& pos, int& see);
	size_t count() const { return cnt; }
	bool done() const { return idx == cnt; }
private:
	Move moves[MAX_MOVES];
	int scores[MAX_MOVES];
	size_t cnt, idx;

	void generate(const Position& pos, Phase ph);
	void score(const Position& pos);
};

}	// namespace sort
