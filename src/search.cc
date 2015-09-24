/*
 * Demolito, a UCI chess engine.
 * Copyright 2015 Lucas Braesch.
 *
 * Demolito is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Demolito is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If
 * not, see <http://www.gnu.org/licenses/>.
*/
#include <thread>
#include <vector>
#include <chrono>
#include "search.h"
#include "sort.h"
#include "eval.h"
#include "uci.h"

namespace search {

using namespace std::chrono;

std::atomic<uint64_t> nodeCount;	// global node counter
std::atomic<bool> abort;		// all threads should abort flag
class Abort {};				// exception raised in each thread to trigger abortion

template <Phase ph>
int recurse(const Position& pos, int ply, int depth, int alpha, int beta, Move *pv)
{
	int bestScore = -INF;
	const bool inCheck = pos.checkers();

	nodeCount++;
	Move subtreePv[MAX_PLY - ply];
	pv[0].clear();
	if (abort)
		throw Abort();

	const int eval = inCheck ? -INF : evaluate(pos);

	if (ply >= MAX_PLY)
		return eval;

	// QSearch stand pat
	if (ph == QSEARCH && !inCheck) {
		bestScore = eval;
		if (bestScore > alpha) {
			alpha = bestScore;
			if (bestScore > beta)
				return bestScore;
		}
	}

	// Generate and score moves
	Selector S(pos, ph);

	Move m;
	int see;
	size_t moveCount = 0;
	Position nextPos;
	PinInfo pi(pos);

	// Move loop
	while (!S.done() && alpha < beta) {
		m = S.select(pos, see);
		if (!m.pseudo_is_legal(pos, pi))
			continue;
		moveCount++;

		// Prune losing captures in QSearch
		if (ph == QSEARCH && !inCheck && see < 0)
			continue;

		// Play move
		nextPos.set(pos, m);

		const int nextDepth = depth - 1 /*+ (pos.checkers() != 0)*/;

		// Recursion
		int score;
		if (depth <= -8 && !inCheck)
			score = eval + see;	// guard against QSearch explosion
		else
			score = nextDepth > 0
				? -recurse<SEARCH>(nextPos, ply + 1, nextDepth, -beta, -alpha, subtreePv)
				: -recurse<QSEARCH>(nextPos, ply + 1, nextDepth, -beta, -alpha, subtreePv);

		// Update bestScore and alpha
		if (score > bestScore) {
			bestScore = score;
			if (score > alpha) {
				pv[0] = m;
				for (int i = 0; i < MAX_PLY - ply; i++)
					if ((pv[i + 1] = subtreePv[i]).null())
						break;
				alpha = score;
			}
		}
	}

	// No legal move: mated or stalemated
	if (!moveCount)
		return inCheck ? ply - MATE : 0;

	return bestScore;
}

void iterate(const Position& pos, const Limits& lim, UCI::Info& ui)
{
	Move pv[MAX_PLY + 1];

	for (int depth = 1; depth <= lim.depth; depth++) {
		int score;
		try {
			score = recurse<SEARCH>(pos, 0, depth, -INF, +INF, pv);
		} catch (const Abort&) {
			break;
		}
		ui.update(depth, score, nodeCount, pv);
	}
	abort = true;
}

void bestmove(const Position& pos, const Limits& lim)
{
	auto start = high_resolution_clock::now();

	nodeCount = 0;
	abort = false;
	UCI::Info ui;

	std::vector<std::thread> threads;
	for (int i = 0; i < lim.threads; i++)
		threads.emplace_back(iterate, std::cref(pos), std::cref(lim), std::ref(ui));

	while (!abort) {
		std::this_thread::sleep_for(milliseconds(5));
		ui.print();

		if (lim.nodes && nodeCount >= lim.nodes)
			abort = true;
		else if (lim.movetime && duration_cast<milliseconds>
		(high_resolution_clock::now() - start).count() >= lim.movetime)
			abort = true;
	}

	for (auto& t : threads)
		t.join();

	// FIXME: return best move
}

}	// namespace search
