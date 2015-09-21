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
#include "sort.h"

namespace sort {

void Selector::generate(const Position& pos, Phase ph)
{
	Move *it = moves;

	if (pos.checkers())
		it = gen::check_escapes(pos, it);
	else {
		const int us = pos.turn(), them = opp_color(us);
		const bitboard_t pieceTargets = ph == SEARCH ? ~pos.occ(us) : pos.occ(them);
		const bitboard_t pawnTargets = pieceTargets | pos.ep_square_bb()
			| bb::rank(us == WHITE ? RANK_8 : RANK_1);

		it = gen::piece_moves(pos, it, pieceTargets);
		it = gen::pawn_moves(pos, it, pawnTargets);
		if (ph == SEARCH)
			it = gen::castling_moves(pos, it);
	}

	cnt = it - moves;
}

void Selector::score(const Position& pos)
{
	for (size_t i = 0; i < cnt; i++)
		scores[i] = moves[i].is_tactical(pos) ? moves[i].see(pos) : 0;
}

Selector::Selector(const Position& pos, Phase ph)
{
	generate(pos, ph);
	score(pos);
	idx = 0;
}

Move Selector::select(const Position& pos, int& see)
{
	int maxScore = -INF;
	size_t swapIdx = idx;

	for (size_t i = idx; i < cnt; i++)
		if (scores[i] > maxScore)
			swapIdx = i;

	if (swapIdx != idx) {
		std::swap(moves[idx], moves[swapIdx]);
		std::swap(scores[idx], scores[swapIdx]);
	}

        see = moves[idx].is_tactical(pos) ? scores[idx] : moves[idx].see(pos);
        return moves[idx++];
}

}
