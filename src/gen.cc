/*
 * Demolito, a UCI chess engine.
 * Copyright 2015 Lucas Braesch.
 *
 * Demolito is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Demolito is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include "gen.h"
#include "bitboard.h"

namespace {

template <bool Promotion>
Move *serialize_moves(Move& m, bitboard_t tss, Move *mlist)
{
	while (tss) {
		m.tsq = bb::pop_lsb(tss);
		if (Promotion) {
			for (m.prom = QUEEN; m.prom >= KNIGHT; m.prom--)
				*mlist++ = m;
		} else
			*mlist++ = m;
	}

	return mlist;
}

}	// namespace

namespace gen {

Move *pawn_moves(const Position& pos, Move *mlist, bitboard_t targets)
{
	int us = pos.get_turn(), them = opp_color(us), push = push_inc(us);
	bitboard_t enemies = pos.get_all(them);
	bitboard_t fss, tss;
	Move m;

	if (square_ok(pos.get_ep_square()))
		bb::set(enemies, pos.get_ep_square());

	// Non promotions
	fss = pos.get(us, PAWN) & ~bb::relative_rank(us, RANK_7);
	while (fss) {
                m.fsq = bb::pop_lsb(fss);

		// Calculate to squares: captures, single pushes and double pushes
		tss = bb::pattacks(us, m.fsq) & enemies & targets;
		if (bb::test(~pos.get_all(), m.fsq + push)) {
			if (bb::test(targets, m.fsq + push))
				bb::set(tss, m.fsq + push);
			if (relative_rank(us, m.fsq) == RANK_2 && bb::test(targets & ~pos.get_all(), m.fsq + 2 * push))
				bb::set(tss, m.fsq + 2 * push);
		}

		// Generate moves
		m.prom = NB_PIECE;
		mlist = serialize_moves<false>(m, tss, mlist);
	}

	// Promotions
	fss = pos.get(us, PAWN) & bb::relative_rank(us, RANK_7);
	while (fss) {
                m.fsq = bb::pop_lsb(fss);

		// Calculate to squares: captures and single pushes
		tss = bb::pattacks(us, m.fsq) & enemies & targets;
		if (bb::test(targets & ~pos.get_all(), m.fsq + push))
			bb::set(tss, m.fsq + push);

		// Generate moves (or promotions)
		mlist = serialize_moves<true>(m, tss, mlist);
	}

	return mlist;
}

Move *piece_moves(const Position& pos, Move *mlist, bitboard_t targets)
{
	int us = pos.get_turn();
	bitboard_t fss, tss;

	Move m;
	m.prom = NB_PIECE;

	// King moves
	m.fsq = pos.king_square(us);
	tss = bb::kattacks(m.fsq) & targets;
	mlist = serialize_moves<false>(m, tss, mlist);

	// Knight moves
	fss = pos.get(us, KNIGHT);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::nattacks(m.fsq) & targets;
		mlist = serialize_moves<false>(m, tss, mlist);
	}

	// Rook moves
	fss = pos.get_RQ(us);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::rattacks(m.fsq, pos.get_all()) & targets;
		mlist = serialize_moves<false>(m, tss, mlist);
	}

	// Bishop moves
	fss = pos.get_BQ(us);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::battacks(m.fsq, pos.get_all()) & targets;
		mlist = serialize_moves<false>(m, tss, mlist);
	}

	return mlist;
}

Move *castling_moves(const Position& pos, Move *mlist)
{
	Move m;
	m.fsq = pos.king_square(pos.get_turn());
	m.prom = NB_PIECE;

	bitboard_t tss = pos.get_castlable_rooks();
	while (tss) {
		m.tsq = bb::pop_lsb(tss);
		if (bb::count(bb::segment(m.fsq, m.tsq) & pos.get_all()) == 2)
			*mlist++ = m;
	}

	return mlist;
}

}	// namespace gen
