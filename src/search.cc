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
#include "search.h"
#include "sort.h"
#include "eval.h"

namespace {

struct Stack {
	Move best;
	int ply, eval;
};

template <sort::Phase ph>
int recurse(const Position& pos, Stack *ss, int depth, int alpha, int beta)
{
	int bestScore = -INF;
	const bool inCheck = pos.checkers();
	ss->best.clear();

	search::nodes.fetch_add(1, std::memory_order_relaxed);

	ss->eval = inCheck ? -INF : evaluate(pos);

	if (ss->ply >= MAX_PLY)
		return ss->eval;

	// QSearch stand pat
	if (ph == sort::QSEARCH && !inCheck) {
		bestScore = ss->eval;
		if (bestScore > alpha) {
			alpha = bestScore;
			if (bestScore > beta)
				return bestScore;
		}
	}

	// Generate and score moves
	sort::Selector<ph> S(pos);

	Move m;
	size_t moveCount = 0;
	Position nextPos;
	PinInfo pi(pos);

	// Move loop
	while (!S.done() && alpha < beta) {
		m = S.select();
		if (!m.pseudo_is_legal(pos, pi))
			continue;
		moveCount++;

		// Play move
		nextPos.set(pos, m);

		const int nextDepth = depth - 1 /*+ (pos.checkers() != 0)*/;

		// Recursion
		int score;
		if (depth <= -8 && !inCheck)
			score = ss->eval + m.see(pos);	// guard against qsearch explosion
		else
			score = nextDepth > 0
				? -recurse<sort::SEARCH>(nextPos, ss + 1, nextDepth, -beta, -alpha)
				: -recurse<sort::QSEARCH>(nextPos, ss + 1, nextDepth, -beta, -alpha);

		// Update bestScore and alpha
		if (score > bestScore) {
			bestScore = score;
			if (score > alpha) {
				ss->best = m;
				alpha = score;
			}
		}
	}

	// No legal move: mated or stalemated
	if (!moveCount)
		return inCheck ? ss->ply - MATE : 0;

	return bestScore;
}

}	// namespace

namespace search {

std::atomic<uint64_t> nodes;

Move bestmove(const Position& pos, const Limits& lim)
{
	nodes = 0;

	Stack ss[MAX_PLY];
	for (int ply = 0; ply < MAX_PLY; ply++)
		ss[ply].ply = ply;

	for (int depth = 1; depth <= lim.depth; depth++) {
		const int score = recurse<sort::SEARCH>(pos, ss, depth, -INF, +INF);
		std::cout << "info depth " << depth << " score " << score << " nodes " << nodes
			<< " pv " << ss->best.to_string() << std::endl;
	}

	return ss->best;
}

}	// namespace search
