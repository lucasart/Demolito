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
#include "move.h"
#include "bitboard.h"
#include "position.h"

bitboard_t PinInfo::hidden_checkers(const Position& pos, int attacker, int blocker) const
{
	assert(color_ok(attacker) && color_ok(blocker));
	const int ksq = pos.king_square(opp_color(attacker));
	bitboard_t pinners = (pos.occ_RQ(attacker) & bb::rpattacks(ksq))
		| (pos.occ_BQ(attacker) & bb::bpattacks(ksq));

	bitboard_t result = 0;
	while (pinners) {
		const int sq = bb::pop_lsb(pinners);
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
	const int us = pos.turn(), them = opp_color(us);
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

bool Move::is_tactical(const Position& pos) const
{
	const int us = pos.turn(), them = opp_color(us);
	return (bb::test(pos.occ(them), tsq))
		|| ((tsq == pos.ep_square() || relative_rank(us, tsq) == RANK_8)
		&& (pos.piece_on(fsq) == PAWN));
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

bool Move::pseudo_is_legal(const Position& pos, const PinInfo& pi) const
{
	const int piece = pos.piece_on(fsq);
	const int ksq = pos.king_square(pos.turn());

	if (piece == KING) {
		if (bb::test(pos.occ(pos.turn()), tsq)) {
			// Castling: king must not move through attacked square, and rook must not
			// be pinned
			assert(pos.piece_on(tsq) == ROOK);
			const int _tsq = square(rank_of(fsq), fsq < tsq ? FILE_G : FILE_C);
			return !(pos.attacked() & bb::segment(fsq, _tsq))
				&& !bb::test(pi.pinned, tsq);
		} else
			// Normal king move: do not land on an attacked square
			return !bb::test(pos.attacked(), tsq);
	} else {
		// Normal case: illegal if pinned, and moves out of pin-ray
		if (bb::test(pi.pinned, fsq) && !bb::test(bb::ray(ksq, fsq), tsq))
			return false;

		// En-passant special case: also illegal if self-check through the en-passant
		// captured pawn
		if (tsq == pos.ep_square() && piece == PAWN) {
			const int us = pos.turn(), them = opp_color(us);
			bitboard_t occ = pos.occ();
			bb::clear(occ, fsq);
			bb::set(occ, tsq);
			bb::clear(occ, tsq + push_inc(them));
			return !(bb::rattacks(ksq, occ) & pos.occ_RQ(them))
				&& !(bb::battacks(ksq, occ) & pos.occ_BQ(them));
		} else
			return true;
	}
}

int Move::see(const Position& pos) const
{
	const int see_value[NB_PIECE+1] = {N, B, R, Q, 20 * Q, P, 0};

	int us = pos.turn();
	bitboard_t occ = pos.occ();

	// General case
	int gain[32] = {see_value[pos.piece_on(tsq)]};
	int capture = pos.piece_on(fsq);
	bb::clear(occ, fsq);

	// Special cases
	if (capture == PAWN) {
		if (tsq == pos.ep_square()) {
			bb::clear(occ, tsq - push_inc(us));
			gain[0] = see_value[capture];
		} else if (relative_rank(us, tsq) == RANK_8)
			gain[0] += see_value[capture = prom] - see_value[PAWN];
	}

	// Easy case: tsq is not defended
	// TODO: explore performance tradeoff between using pos.attacked() and using attackers below
	if (!bb::test(pos.attacked(), tsq))
		return gain[0];

	bitboard_t attackers = pos.attackers_to(tsq, occ);
	bitboard_t our_attackers;

	int idx = 0;
	while (us = opp_color(us), our_attackers = attackers & pos.occ(us)) {
		// Find least valuable attacker (LVA)
		int piece = PAWN;
		if (!(our_attackers & pos.occ(us, PAWN))) {
			for (piece = KNIGHT; piece <= KING; piece++) {
				if (our_attackers & pos.occ(us, piece))
					break;
			}
		}

		// Remove the LVA
		bb::clear(occ, bb::lsb(our_attackers & pos.occ(us, piece)));

		// Scan for new X-ray attacks through the LVA
		if (piece != KNIGHT) {
			attackers |= (pos.by_piece(BISHOP) | pos.by_piece(QUEEN))
				& bb::bpattacks(tsq) & bb::battacks(tsq, occ);
			attackers |= (pos.by_piece(ROOK) | pos.by_piece(QUEEN))
				& bb::rpattacks(tsq) & bb::rattacks(tsq, occ);
		}

		// Remove attackers we've already done
		attackers &= occ;

		// Add the new entry to the gain[] array
		idx++;
		assert(idx < 32);
		gain[idx] = see_value[capture] - gain[idx-1];

		if (piece == PAWN && relative_rank(us, tsq) == RANK_8) {
			gain[idx] += see_value[QUEEN] - see_value[PAWN];
			capture = QUEEN;
		} else
			capture = piece;
	}

	do {
		if (-gain[idx] < gain[idx-1])
			gain[idx-1] = -gain[idx];
	} while (--idx);

	return gain[0];
}
