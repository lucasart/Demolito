#include <iostream>
#include <sstream>
#include "uci.h"

namespace UCI {

void Info::clear()
{
	m.lock();
	_depth = _score = _nodes = 0;
	_pv[0].clear();
	updated = false;
	m.unlock();
}

void Info::update(int depth, int score, int nodes, Move *pv)
{
	m.lock();
	if (depth >= _depth) {
		_depth = depth;
		_score = score;
		_nodes = nodes;
		for (int i = 0; ; i++)
			if ((_pv[i] = pv[i]).null())
				break;
		updated = true;
	}
	m.unlock();
}

// print info line, only if it has been updated since last print()
void Info::print() const
{
	std::ostringstream os;

	m.lock();
	if (updated) {
		os << "info depth " << _depth << " score " << _score << " nodes " << _nodes << " pv ";
		for (int i = 0; !_pv[i].null(); i++)
			os << _pv[i].to_string() << ' ';
		std::cout << os.str() << std::endl;
		updated = false;
	}
	m.unlock();
}

}	// namespace UCI
