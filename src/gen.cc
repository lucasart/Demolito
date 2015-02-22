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
#include <iostream>
#include "gen.h"
#include "bitboard.h"

namespace {

template <bool Promotion>
Move *serialize_moves(Move& m, bitboard_t tss, const PinInfo& pi, Move *mlist)
{
	if (bb::test(pi.pinned, m.fsq))
		tss &= bb::ray(pi.ksq, m.fsq);

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

Move *pawn_moves(const Position& pos, const PinInfo& pi, Move *mlist, bitboard_t targets)
{
	assert(!pos.checkers());
	int us = pos.turn(), them = opp_color(us), push = push_inc(us);
	bitboard_t theirs = pos.occ(them);
	bitboard_t fss, tss;
	Move m;

	// Non promotions
	fss = pos.occ(us, PAWN) & ~bb::relative_rank(us, RANK_7);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);

		// Calculate to squares: captures, single pushes and double pushes
		tss = bb::pattacks(us, m.fsq) & theirs & targets;
		if (bb::test(~pos.occ(), m.fsq + push)) {
			if (bb::test(targets, m.fsq + push))
				bb::set(tss, m.fsq + push);
			if (relative_rank(us, m.fsq) == RANK_2
			&& bb::test(targets & ~pos.occ(), m.fsq + 2 * push))
				bb::set(tss, m.fsq + 2 * push);
		}

		// Generate moves
		m.prom = NB_PIECE;
		mlist = serialize_moves<false>(m, tss, pi, mlist);
	}

	// Promotions
	fss = pos.occ(us, PAWN) & bb::relative_rank(us, RANK_7);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);

		// Calculate to squares: captures and single pushes
		tss = bb::pattacks(us, m.fsq) & theirs & targets;
		if (bb::test(targets & ~pos.occ(), m.fsq + push))
			bb::set(tss, m.fsq + push);

		// Generate moves (or promotions)
		mlist = serialize_moves<true>(m, tss, pi, mlist);
	}

	// En passant
	if (square_ok(pos.ep_square()) && bb::test(targets, pos.ep_square())) {
		m.prom = NB_PIECE;
		m.tsq = pos.ep_square();
		fss = bb::pattacks(them, m.tsq) & pos.occ(us, PAWN);
		assert(fss);	// ep-square is only set if attackable by pawns

		while (fss) {
			m.fsq = bb::pop_lsb(fss);

			// Play the ep-capture on the occupancy copy
			bitboard_t occ = pos.occ();
			bb::clear(occ, m.fsq);
			bb::clear(occ, m.tsq + push_inc(them));
			bb::set(occ, m.tsq);

			// If no enemy slider checks us with the new occupancy, en-passant is legal
			if (!(bb::rattacks(pi.ksq, occ) & pos.occ_RQ(them))
			&& !(bb::battacks(pi.ksq, occ) & pos.occ_BQ(them)))
				*mlist++ = m;
		}
	}

	return mlist;
}

Move *piece_moves(const Position& pos, const PinInfo& pi, Move *mlist, bitboard_t targets,
	bool kingMoves)
{
	assert(!pos.checkers());
	int us = pos.turn();
	bitboard_t fss, tss;

	Move m;
	m.prom = NB_PIECE;

	// King moves
	if (kingMoves) {
		m.fsq = pos.king_square(us);
		tss = bb::kattacks(m.fsq) & targets;
		mlist = serialize_moves<false>(m, tss, pi, mlist);
	}

	// Knight moves
	fss = pos.occ(us, KNIGHT);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::nattacks(m.fsq) & targets;
		mlist = serialize_moves<false>(m, tss, pi, mlist);
	}

	// Rook moves
	fss = pos.occ_RQ(us);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::rattacks(m.fsq, pos.occ()) & targets;
		mlist = serialize_moves<false>(m, tss, pi, mlist);
	}

	// Bishop moves
	fss = pos.occ_BQ(us);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::battacks(m.fsq, pos.occ()) & targets;
		mlist = serialize_moves<false>(m, tss, pi, mlist);
	}

	return mlist;
}

Move *castling_moves(const Position& pos, const PinInfo& pi, Move *mlist)
{
	assert(!pos.checkers());
	Move m;
	m.fsq = pos.king_square(pos.turn());
	m.prom = NB_PIECE;

	bitboard_t tss = pos.castlable_rooks() & pos.occ(pos.turn());
	while (tss) {
		m.tsq = bb::pop_lsb(tss);
		if (bb::count(bb::segment(m.fsq, m.tsq) & pos.occ()) == 2 && !bb::test(pi.pinned, m.tsq))
			*mlist++ = m;
	}

	return mlist;
}

Move *check_escapes(const Position& pos, const PinInfo& pi, Move *mlist)
{
	assert(pos.checkers());
	bitboard_t ours = pos.occ(pos.turn());
	bitboard_t tss;
	Move m;

	// King moves
	tss = bb::kattacks(pi.ksq) & ~ours;
	m.prom = NB_PIECE;
	mlist = serialize_moves<false>(m, tss, pi, mlist);

	if (!bb::several(pos.checkers())) {
		// Single checker
		int checkerSquare = bb::lsb(pos.checkers());
		int checkerPiece = pos.piece_on(checkerSquare);

		// Piece moves must cover the checking segment for a sliding check,
		// or capture the checker otherwise.
		tss = BISHOP <= checkerPiece && checkerPiece <= QUEEN
			? bb::segment(pi.ksq, checkerSquare)
			: pos.checkers();

		mlist = piece_moves(pos, pi, mlist, ~ours, false);

		// if checked by a Pawn and epsq is available, then the check must result
		// from a pawn double push, and we also need to consider capturing it en
		// passant to solve the check
		if (checkerPiece == PAWN && square_ok(pos.ep_square()))
			bb::set(tss, pos.ep_square());

		mlist = pawn_moves(pos, pi, mlist, tss);
	}

	return mlist;
}

Move *all_moves(const Position& pos, const PinInfo& pi, Move *mlist)
{
	if (pos.checkers())
		return check_escapes(pos, pi, mlist);
	else {
		bitboard_t targets = ~pos.occ(pos.turn());
		Move *m = mlist;

		m = pawn_moves(pos, pi, m, targets);
		m = piece_moves(pos, pi, m, targets);
		m = castling_moves(pos, pi, m);
		return m;
	}
}

}	// namespace gen
