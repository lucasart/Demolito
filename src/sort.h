#pragma once
#include "gen.h"

namespace sort {

enum Phase {SEARCH, QSEARCH};

template <Phase>
class Selector {
public:
	Selector(const Position& pos);
	Move select();
	size_t count() const { return cnt; }
	bool done() const { return idx == cnt; }
private:
	Move moves[MAX_MOVES];
	int scores[MAX_MOVES];
	size_t cnt, idx;

	void generate(const Position& pos);
	void score(const Position& pos);
};

}	// namespace sort
