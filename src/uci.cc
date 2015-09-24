#include <iostream>
#include <sstream>
#include "uci.h"

namespace UCI {

Info::Info() : _depth(0), updated(false)
{
	_pv[0].clear();
}

void Info::update(int depth, int score, int nodes, Move *pv)
{
	std::lock_guard<std::mutex> lk(m);
	if (depth > _depth) {
		_depth = depth;
		_score = score;
		_nodes = nodes;
		for (int i = 0; ; i++)
			if ((_pv[i] = pv[i]).null())
				break;
		updated = true;
	}
}

// print info line, only if it has been updated since last print()
void Info::print() const
{
	std::lock_guard<std::mutex> lk(m);
	if (updated) {
		std::ostringstream os;
		os << "info depth " << _depth << " score " << _score
			<< " nodes " << _nodes << " pv";
		for (int i = 0; !_pv[i].null(); i++)
			os << ' ' << _pv[i].to_string();
		std::cout << os.str() << std::endl;
		updated = false;
	}
}

}	// namespace UCI
