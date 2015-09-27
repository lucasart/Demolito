#pragma once
#include <mutex>
#include "move.h"

namespace UCI {

void loop();

class Info {
	int lastDepth;
	Move best, ponder;
	mutable std::mutex m;
public:
	Info() : lastDepth(0) { best.clear(); ponder.clear(); }
	void update(int depth, int score, int nodes, Move *pv);
	void print_bestmove() const;
};

}	// namespace UCI

