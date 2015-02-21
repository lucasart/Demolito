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
#include <cassert>
#include "move.h"
#include "bitboard.h"
#include "position.h"

bitboard_t PinInfo::hidden_checkers(const Position& pos, int attacker, int blocker) const
{
	assert(color_ok(attacker) && color_ok(blocker));
	int ksq = pos.king_square(opp_color(attacker));
	bitboard_t pinners = (pos.occ_RQ(attacker) & bb::rpattacks(ksq)) | (pos.occ_BQ(attacker) & bb::bpattacks(ksq));

	bitboard_t result = 0;
	while (pinners) {
		int sq = bb::pop_lsb(pinners);
		bitboard_t skewered = bb::segment(ksq, sq) & pos.occ();
		bb::clear(skewered, ksq);
		bb::clear(skewered, sq);

		if (!bb::several(skewered) && (skewered & pos.occ(blocker)))
			result |= skewered;
	}
	return result;
}

PinInfo::PinInfo(const Position& pos)
{
	int us = pos.turn(), them = opp_color(us);
	ksq = pos.king_square(us);
	pinned = hidden_checkers(pos, them, us);
	discoCheckers = hidden_checkers(pos, us, us);
}

bool Move::ok() const
{
	return square_ok(fsq) && square_ok(tsq)
		&& ((KNIGHT <= prom && prom <= QUEEN) || prom == NB_PIECE);
}

move_t Move::encode() const
{
	assert(ok());
	return fsq | (tsq << 6) | (prom << 12);
}

void Move::decode(move_t em)
{
	fsq = em & 077;
	tsq = (em >> 6) & 077;
	prom = em >> 12;
	assert(ok());
}

std::string Move::to_string() const
{
	assert(ok());
	std::string s;

	s += file_of(fsq) + 'a';
	s += rank_of(fsq) + '1';
	s += file_of(tsq) + 'a';
	s += rank_of(tsq) + '1';
	if (piece_ok(prom))
		s += PieceLabel[BLACK][prom];

	return s;
}

void Move::from_string(const std::string& s)
{
	fsq = square(s[1] - '1', s[0] - 'a');
	tsq = square(s[3] - '1', s[2] - 'a');
	prom = s[4] ? (int)PieceLabel[BLACK].find(s[4]) : NB_PIECE;
	assert(ok());
}

bool Move::gives_check(const Position& pos, const PinInfo& pi) const
{
	int us = pos.turn(), them = opp_color(us);
	int ksq = pos.king_square(them);
	int piece = piece_ok(prom) ? prom : pos.piece_on(fsq);

	// Direct check ?
	if (bb::test(bb::piece_attacks(us, piece, tsq, pos.occ()), ksq))
		return true;

	// Disco check ?
	if (bb::test(pi.discoCheckers, fsq) && !bb::test(bb::ray(ksq, fsq), tsq))
		return true;

	// En passant
	if (piece == PAWN && tsq == pos.ep_square()) {
		bitboard_t occ = pos.occ();
		bb::clear(occ, fsq);
		bb::set(occ, tsq);
		bb::clear(occ, tsq + push_inc(them));
		return (pos.occ_RQ(us) & bb::rattacks(ksq, occ)) || (pos.occ_BQ(us) & bb::battacks(ksq, occ));
	}

	// Castling
	if (piece == KING && bb::test(pos.occ(us), tsq)) {
		bitboard_t occ = pos.occ();
		bb::clear(occ, fsq);
		bb::set(occ, square(rank_of(fsq), tsq > fsq ? FILE_G : FILE_C));
		return (pos.occ_RQ(us) & bb::rattacks(ksq, occ)) || (pos.occ_BQ(us) & bb::battacks(ksq, occ));
	}

	return false;
}
