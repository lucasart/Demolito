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
	int ply;
};

template <sort::Phase ph>
int search(const Position& pos, Stack *ss, int depth, int alpha, int beta)
{
	int bestScore = -INF;
	const bool inCheck = pos.checkers();
	ss->best.clear();

	// QSearch stand pat
	if (ph == sort::QSEARCH && !inCheck) {
		bestScore = evaluate(pos);
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
		nextPos.play(pos, m);

		const int nextDepth = depth - 1 /*+ (pos.checkers() != 0)*/;

		// Recursion
		const int score = nextDepth > 0
			? -search<sort::SEARCH>(nextPos, ss + 1, nextDepth, -beta, -alpha)
			: -search<sort::QSEARCH>(nextPos, ss + 1, nextDepth, -beta, -alpha);

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

Move bestmove(const Position& pos)
{
	Stack ss[MAX_PLY];
	for (int ply = 0; ply < MAX_PLY; ply++)
		ss[ply].ply = ply;

	for (int depth = 1; depth < MAX_PLY; depth++) {
		const int score = search<sort::SEARCH>(pos, ss, depth, -INF, +INF);
		std::cout << "info depth " << depth << " score " << score
			<< " pv " << ss->best.to_string() << std::endl;
	}

	return ss->best;
}
