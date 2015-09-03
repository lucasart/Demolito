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
#include "search.h"
#include "eval.h"

template <sort::Phase ph>
int search(const Position& pos, int depth, int ply, int alpha, int beta)
{
	int bestScore = -INF;
	const bool inCheck = pos.checkers();

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
	Position nextPos;

	// Move loop
	while ((m = S.select()) && (alpha < beta)) {

		// Play move
		nextPos.play(pos, m);

		const int nextDepth = depth - 1 /*+ (pos.checkers() != 0)*/;

		// Recursion
		const int score = nextDepth > 0
			? -search<sort::SEARCH>(nextPos, nextDepth, ply + 1, -beta, -alpha)
			: -search<sort::QSEARCH>(nextPos, nextDepth, ply + 1, -beta, -alpha);

		// Update bestScore and alpha
		if (score > bestScore) {
			bestScore = score;
			if (score > alpha)
				alpha = score;
		}
	}

	// No legal move: mated or stalemated
	if (!S.count())
		return inCheck ? ply - MATE : 0;

	return bestScore;
}
