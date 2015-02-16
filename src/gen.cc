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

namespace gen {

Move *piece_moves(const Position& pos, Move *mlist, bitboard_t targets)
{
	int us = pos.get_turn();
	bitboard_t fss, tss;

	Move m;
	m.prom = NB_PIECE;

	// King moves
	m.fsq = pos.king_square(us);
	tss = bb::kattacks(m.fsq) & targets;
	while (tss) {
		m.tsq = bb::pop_lsb(tss);
		*mlist++ = m;
		std::cout << m.to_string() << std::endl;
	}

	// Knight moves
	fss = pos.get(us, KNIGHT);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::nattacks(m.fsq) & targets;
		while (tss) {
			m.tsq = bb::pop_lsb(tss);
			*mlist++ = m;
			std::cout << m.to_string() << std::endl;
		}
	}

	// Rook moves
	fss = pos.get_RQ(us);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::rattacks(m.fsq, pos.get_all()) & targets;
		while (tss) {
			m.tsq = bb::pop_lsb(tss);
			*mlist++ = m;
			std::cout << m.to_string() << std::endl;
		}
	}

	// Bishop moves
	fss = pos.get_BQ(us);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::battacks(m.fsq, pos.get_all()) & targets;
		while (tss) {
			m.tsq = bb::pop_lsb(tss);
			*mlist++ = m;
			std::cout << m.to_string() << std::endl;
		}
	}

	return mlist;
}

}	// namespace gen
