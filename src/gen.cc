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
#include "gen.h"
#include "move.h"
#include "bitboard.h"

namespace {

template <bool Promotion>
Move *serialize_moves(Move& m, bitboard_t tss, Move *mList, bool subPromotions = true)
{
	while (tss) {
		m.tsq = bb::pop_lsb(tss);
		if (Promotion) {
			if (subPromotions) {
				for (m.prom = QUEEN; m.prom >= KNIGHT; m.prom--)
					*mList++ = m;
			} else {
				m.prom = QUEEN;
				*mList++ = m;
			}
		} else
			*mList++ = m;
	}

	return mList;
}

}	// namespace

namespace gen {

Move *pawn_moves(const Position& pos, Move *mList, bitboard_t targets, bool subPromotions)
{
	const int us = pos.turn(), them = opp_color(us), push = push_inc(us);
	const bitboard_t capturable = pos.occ(them) | pos.ep_square_bb();
	bitboard_t fss, tss;
	Move m;

	// Non promotions
	fss = pos.occ(us, PAWN) & ~bb::relative_rank(us, RANK_7);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);

		// Calculate to squares: captures, single pushes and double pushes
		tss = bb::pattacks(us, m.fsq) & capturable & targets;
		if (bb::test(~pos.occ(), m.fsq + push)) {
			if (bb::test(targets, m.fsq + push))
				bb::set(tss, m.fsq + push);
			if (relative_rank(us, m.fsq) == RANK_2
			&& bb::test(targets & ~pos.occ(), m.fsq + 2 * push))
				bb::set(tss, m.fsq + 2 * push);
		}

		// Generate moves
		m.prom = NB_PIECE;
		mList = serialize_moves<false>(m, tss, mList);
	}

	// Promotions
	fss = pos.occ(us, PAWN) & bb::relative_rank(us, RANK_7);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);

		// Calculate to squares: captures and single pushes
		tss = bb::pattacks(us, m.fsq) & capturable & targets;
		if (bb::test(targets & ~pos.occ(), m.fsq + push))
			bb::set(tss, m.fsq + push);

		// Generate moves (or promotions)
		mList = serialize_moves<true>(m, tss, mList, subPromotions);
	}

	return mList;
}

Move *piece_moves(const Position& pos, Move *mList, bitboard_t targets, bool kingMoves)
{
	const int us = pos.turn();
	bitboard_t fss, tss;

	Move m;
	m.prom = NB_PIECE;

	// King moves
	if (kingMoves) {
		m.fsq = pos.king_square(us);
		tss = bb::kattacks(m.fsq) & targets;
		mList = serialize_moves<false>(m, tss, mList);
	}

	// Knight moves
	fss = pos.occ(us, KNIGHT);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::nattacks(m.fsq) & targets;
		mList = serialize_moves<false>(m, tss, mList);
	}

	// Rook moves
	fss = pos.occ_RQ(us);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::rattacks(m.fsq, pos.occ()) & targets;
		mList = serialize_moves<false>(m, tss, mList);
	}

	// Bishop moves
	fss = pos.occ_BQ(us);
	while (fss) {
		m.fsq = bb::pop_lsb(fss);
		tss = bb::battacks(m.fsq, pos.occ()) & targets;
		mList = serialize_moves<false>(m, tss, mList);
	}

	return mList;
}

Move *castling_moves(const Position& pos, Move *mList)
{
	assert(!pos.checkers());
	Move m;
	m.fsq = pos.king_square(pos.turn());
	m.prom = NB_PIECE;

	bitboard_t tss = pos.castlable_rooks() & pos.occ(pos.turn());
	while (tss) {
		m.tsq = bb::pop_lsb(tss);
		const int ktsq = square(rank_of(m.tsq), m.tsq > m.fsq ? FILE_G : FILE_C);
		const bitboard_t s = bb::segment(m.fsq, m.tsq) | bb::segment(m.fsq, ktsq);
		if (bb::count(s & pos.occ()) == 2)
			*mList++ = m;
	}

	return mList;
}

Move *check_escapes(const Position& pos, Move *mList, bool subPromotions)
{
	assert(pos.checkers());
	bitboard_t ours = pos.occ(pos.turn());
	const int ksq = pos.king_square(pos.turn());
	bitboard_t tss;
	Move m;

	// King moves
	tss = bb::kattacks(ksq) & ~ours;
	m.fsq = ksq;
	m.prom = NB_PIECE;
	mList = serialize_moves<false>(m, tss, mList);

	if (!bb::several(pos.checkers())) {
		// Single checker
		const int checkerSquare = bb::lsb(pos.checkers());
		const int checkerPiece = pos.piece_on(checkerSquare);

		// Piece moves must cover the checking segment for a sliding check, or capture the
		// checker otherwise.
		tss = BISHOP <= checkerPiece && checkerPiece <= QUEEN
			? bb::segment(ksq, checkerSquare)
			: pos.checkers();

		mList = piece_moves(pos, mList, tss & ~ours, false);

		// if checked by a Pawn and epsq is available, then the check must result from a
		// pawn double push, and we also need to consider capturing it en-passant to solve
		// the check.
		if (checkerPiece == PAWN && square_ok(pos.ep_square()))
			bb::set(tss, pos.ep_square());

		mList = pawn_moves(pos, mList, tss, subPromotions);
	}

	return mList;
}

Move *all_moves(const Position& pos, Move *mList)
{
	if (pos.checkers())
		return check_escapes(pos, mList);
	else {
		bitboard_t targets = ~pos.occ(pos.turn());
		Move *m = mList;

		m = pawn_moves(pos, m, targets);
		m = piece_moves(pos, m, targets);
		m = castling_moves(pos, m);
		return m;
	}
}

template <bool Root>
uint64_t perft(const Position& pos, int depth)
{
	// Do not use bulk-counting. It's faster, but falses profiling results.
	if (depth <= 0)
		return 1;

	uint64_t result = 0;
	Position after;
	const PinInfo pi(pos);
	Move mList[MAX_MOVES];
	Move *end = all_moves(pos, mList);

	for (Move *m = mList; m != end; m++) {
		if (!m->pseudo_is_legal(pos, pi))
			continue;

		after.set(pos, *m);
		const uint64_t sub_tree = perft<false>(after, depth - 1);
		result += sub_tree;

		if (Root)
			std::cout << m->to_string() << '\t' << sub_tree << std::endl;
	}

	return result;
}

template uint64_t perft<true>(const Position& pos, int depth);

}	// namespace gen
