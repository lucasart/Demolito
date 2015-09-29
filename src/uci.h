#pragma once
#include <mutex>
#include "move.h"
#include "zobrist.h"

namespace uci {

extern zobrist::History history;

void loop();

class Info {
	int lastDepth;
	Move best, ponder;
	mutable std::mutex m;
public:
	Info() : lastDepth(0) { best.clear(); ponder.clear(); }
	int last_depth() const { return lastDepth; }
	void update(int depth, int score, int nodes, Move *pv);
	void print_bestmove() const;
};

}	// namespace UCI

