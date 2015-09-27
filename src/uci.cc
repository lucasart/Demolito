#include <iostream>
#include <sstream>
#include "uci.h"
#include "eval.h"

namespace {

Position pos;

void position(std::istringstream& is)
{
	std::string token, fen;
	is >> token;

	if (token == "startpos") {
		fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
		is >> token;	// consume "moves" token (if present)
	} else if (token == "fen") {
		while (is >> token && token != "moves")
			fen += token + " ";
	} else
		return;

	Position p[NB_COLOR];
	int idx = 0;
	p[idx].set(fen);

	// Parse moves (if any)
	while (is >> token) {
		const Move m(token);
		p[idx ^ 1].set(p[idx], m);
		idx ^= 1;
	}

	pos = p[idx];
}

void eval()
{
	pos.print();
	std::cout << "eval = " << evaluate(pos) << std::endl;
}

}	// namespace

namespace UCI {

void loop()
{
	std::string command, token;

	while (std::getline(std::cin, command) && command != "quit") {
		std::istringstream is(command);
		is >> token;

		if (token == "uci")
			std::cout << "uciok" << std::endl;
		else if (token == "isready")
			std::cout << "readyok" << std::endl;
		else if (token == "ucinewgame")
			;
		else if (token == "position")
			position(is);
		else if (token == "eval")
			eval();
		else
			std::cout << "unknown command: " << command << std::endl;
	}
}

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
		os << "info depth " << _depth << " score " << _score / 2
			<< " nodes " << _nodes << " pv";
		for (int i = 0; !_pv[i].null(); i++)
			os << ' ' << _pv[i].to_string();
		std::cout << os.str() << std::endl;
		updated = false;
	}
}

Move Info::best(Move& ponder) const
{
	std::lock_guard<std::mutex> lk(m);
	ponder = _pv[1];
	return _pv[0];
}

}	// namespace UCI
