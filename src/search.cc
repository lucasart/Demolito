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
#include <iostream>
#include <thread>
#include <vector>
#include "search.h"
#include "sort.h"
#include "eval.h"

namespace {

struct Stack {
	Move best;
	int eval;
};

thread_local Stack ss[MAX_PLY];

template <sort::Phase ph>
int recurse(const Position& pos, int ply, int depth, int alpha, int beta)
{
	int bestScore = -INF;
	const bool inCheck = pos.checkers();
	ss[ply].best.clear();

	search::nodes.fetch_add(1, std::memory_order_relaxed);

	ss[ply].eval = inCheck ? -INF : evaluate(pos);

	if (ply >= MAX_PLY)
		return ss[ply].eval;

	// QSearch stand pat
	if (ph == sort::QSEARCH && !inCheck) {
		bestScore = ss[ply].eval;
		if (bestScore > alpha) {
			alpha = bestScore;
			if (bestScore > beta)
				return bestScore;
		}
	}

	// Generate and score moves
	sort::Selector S(pos, ph);

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

		// Play move
		nextPos.set(pos, m);

		const int nextDepth = depth - 1 /*+ (pos.checkers() != 0)*/;

		// Recursion
		int score;
		if (depth <= -8 && !inCheck)
			score = ss[ply].eval + see;	// guard against qsearch explosion
		else
			score = nextDepth > 0
				? -recurse<sort::SEARCH>(nextPos, ply + 1, nextDepth, -beta, -alpha)
				: -recurse<sort::QSEARCH>(nextPos, ply + 1, nextDepth, -beta, -alpha);

		// Update bestScore and alpha
		if (score > bestScore) {
			bestScore = score;
			if (score > alpha) {
				ss[ply].best = m;
				alpha = score;
			}
		}
	}

	// No legal move: mated or stalemated
	if (!moveCount)
		return inCheck ? ply - MATE : 0;

	return bestScore;
}

}	// namespace

namespace search {

std::atomic<uint64_t> nodes;

void iterate(const Position& pos, const Limits& lim)
{
	for (int depth = 1; depth <= lim.depth; depth++) {
		const int score = recurse<sort::SEARCH>(pos, 0, depth, -INF, +INF);
		std::cout << "info depth " << depth << " score " << score << " nodes " << nodes
			<< " pv " << ss->best.to_string() << std::endl;
	}
}

Move bestmove(const Position& pos, const Limits& lim)
{
	nodes = 0;

	std::vector<std::thread> threads;
	for (int i = 0; i < lim.threads; i++)
		threads.emplace_back(iterate, std::cref(pos), std::cref(lim));

	for (auto& t : threads)
		t.join();

	return ss[0].best;
}

}	// namespace search
