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
#include <sstream>
#include <cstring>	// std::memset
#include "position.h"
#include "bitboard.h"
#include "zobrist.h"

bitboard_t Position::occupied() const
{
	return byColor[WHITE] | byColor[BLACK];
}

bitboard_t Position::occupied(int color) const
{
	return byColor[color];
}

bool Position::key_ok() const
{
	uint64_t k = 0;

	for (int color = 0; color < NB_COLOR; color++)
		for (int piece = 0; piece < NB_PIECE; piece++) {
			bitboard_t b = get(color, piece);
			while (b)
				k ^= zobrist::key(color, piece, bb::pop_lsb(b));
		}

	if (turn == BLACK)
		k ^= zobrist::turn();

	return k == key;
}

void Position::clear()
{
	std::memset(this, 0, sizeof(*this));
}

void Position::clear(int color, int piece, int sq)
{
	assert(color_ok(color) && piece_ok(piece) && square_ok(sq));
	bb::clear(byColor[color], sq);
	bb::clear(byPiece[piece], sq);
	key ^= zobrist::key(color, piece, sq);
}

void Position::set(int color, int piece, int sq)
{
	assert(color_ok(color) && piece_ok(piece) && square_ok(sq));
	bb::set(byColor[color], sq);
	bb::set(byPiece[piece], sq);
	key ^= zobrist::key(color, piece, sq);
}

void Position::set_pos(const std::string& fen)
{
	clear();
	std::istringstream is(fen);
	std::string s;

	// Piece placement
	is >> s;
	int sq = A8;
	for (char c : s) {
		if (isdigit(c))
			sq += c - '0';
		else if (c == '/')
			sq -= 16;
		else {
			for (int color = 0; color < NB_COLOR; color++) {
				int piece = PieceLabel[color].find(c);
				if (piece_ok(piece))
					set(color, piece, sq++);
			}
		}
	}

	// Turn of play
	is >> s;
	if (s == "w")
		turn = WHITE;
	else {
		turn = BLACK;
		key ^= zobrist::turn();
	}

	// Castling rights
	is >> s;
	for (char c : s) {
		int color = isupper(c) ? WHITE : BLACK;
		int r = RANK_8 * color;
		c = toupper(c);

		if (c == 'K')
			sq = square(r, FILE_H);
		else if (c == 'Q')
			sq = square(r, FILE_A);
		else if ('A' <= c && c <= 'H')
			sq = square(r, c - 'A');

		bb::set(castlableRooks, sq);
	}

	// En passant square
	is >> s;
	epSquare = s != "-"
		? square(s[1] - '1', s[0] - 'a')
		: NB_SQUARE;

	// 50-move counter
	is >> rule50;
}

std::string Position::get_pos() const
{
	std::ostringstream os;

	// Piece placement
	for (int r = RANK_8; r >= RANK_1; r--) {
		int cnt = 0;

		for (int f = FILE_A; f <= FILE_H; f++) {
			int sq = square(r, f);

			if (bb::test(occupied(), sq)) {
				int color = color_on(sq);
				int piece = piece_on(sq);
				if (cnt)
					os << char(cnt + '0');
				cnt = 0;
				os << PieceLabel[color][piece];
			} else
				cnt++;
		}

		if (cnt)
			os << char(cnt + '0');
		os << (r == RANK_1 ? ' ' : '/');
	}

	// Turn of play
	os << (turn == WHITE ? "w " : "b ");

	// Castling rights
	for (int color = WHITE; color <= BLACK; color++) {
		bitboard_t sqs = castlableRooks & byColor[color];
		if (!sqs)
			continue;

		// Because we have castlable rooks, king has to be on the first rank and not in a corner,
		// which allows using bb::ray(ksq, ksq +/- 1) to search for the castle rook in Chess960.
		int ksq = king_square(color);
		assert(rank_of(ksq) == (color == WHITE ? RANK_1 : RANK_8));
		assert(file_of(ksq) != FILE_A && file_of(ksq) != FILE_H);

		// Right side castling
		if (sqs & bb::ray(ksq, ksq + 1)) {
			if (Chess960)
				os << char(file_of(bb::lsb(sqs & bb::ray(ksq, ksq + 1))) + (color == WHITE ? 'A' : 'a'));
			else
				os << PieceLabel[color][KING];
		}

		// Left side castling
		if (sqs & bb::ray(ksq, ksq - 1)) {
			if (Chess960)
				os << char(file_of(bb::msb(sqs & bb::ray(ksq, ksq - 1))) + (color == WHITE ? 'A' : 'a'));
			else
				os << PieceLabel[color][QUEEN];
		}
	}
	os << ' ';

	// En passant
	os << (square_ok(epSquare) ? square_to_string(epSquare) : "-") << ' ';

	// 50 move counter
	os << rule50;

	return os.str();
}

bitboard_t Position::get(int color, int piece) const
{
	assert(color_ok(color) && piece_ok(piece));
	return byColor[color] & byPiece[piece];
}

int Position::get_turn() const
{
	return turn;
}

bitboard_t Position::get_RQ(int color) const
{
	assert(color_ok(color));
	return byColor[color] & (byPiece[ROOK] | byPiece[QUEEN]);
}

bitboard_t Position::get_BQ(int color) const
{
	assert(color_ok(color));
	return byColor[color] & (byPiece[BISHOP] | byPiece[QUEEN]);
}

int Position::color_on(int sq) const
{
	assert(bb::test(occupied(), sq));
	return bb::test(byColor[WHITE], sq) ? WHITE : BLACK;
}

int Position::ep_square() const
{
	return epSquare;
}

int Position::king_square(int color) const
{
	return bb::lsb(get(color, KING));
}

int Position::piece_on(int sq) const
{
	assert(bb::test(occupied(), sq));

	// Pawns first (most frequent)
	if (bb::test(byPiece[PAWN], sq))
		return PAWN;

	// Then pieces in ascending order (Q & K are the rarest, at the end)
	for (int piece = 0; piece < NB_PIECE; piece++)
		if (bb::test(byPiece[piece], sq))
			return piece;

	assert(false);
}

void Position::play(const Position& before, Move m)
{
	*this = before;
	rule50++;

	int us = turn, them = opp_color(us);
	int piece = piece_on(m.fsq);
	int capture = bb::test(occupied(), m.tsq) ? piece_on(m.tsq) : NB_PIECE;

	// Capture piece on to square (if any)
	if (capture != NB_PIECE) {
		rule50 = 0;
		// Use color_on() instead of them, because we could be playing a KxR castling here
		clear(color_on(m.tsq), capture, m.tsq);

		// Capturing a rook alters corresponding castling right
		if (capture == ROOK)
			castlableRooks &= ~(1ULL << m.tsq);
	}

	// Move our piece
	clear(us, piece, m.fsq);
	set(us, piece, m.tsq);

	if (piece == PAWN) {
		// reset rule50, and set epsq
		int push = push_inc(us);
		rule50 = 0;
		epSquare = m.tsq == m.fsq + 2 * push ? m.fsq + push : NB_SQUARE;

		// handle ep-capture and promotion
		if (m.tsq == before.ep_square())
			clear(them, piece, m.tsq - push);
		else if (rank_of(m.tsq) == RANK_8 || rank_of(m.tsq) == RANK_1) {
			clear(us, piece, m.tsq);
			set(us, m.prom, m.tsq);
		}
	} else {
		epSquare = NB_SQUARE;

		if (piece == ROOK)
			// remove corresponding castling right
			castlableRooks &= ~(1ULL << m.fsq);
		else if (piece == KING) {
			// Lose all castling rights
			castlableRooks &= ~bb::rank(us * RANK_8);

			// Castling
			if (bb::test(before.byColor[us], m.tsq)) {
				// Capturing our own piece can only be a castling move, encoded KxR
				assert(before.piece_on(m.tsq) == ROOK);

				int r = rank_of(m.fsq);
				int ksq = m.tsq > m.fsq ? square(r, FILE_G) : square(r, FILE_C);
				int rsq = m.tsq > m.fsq ? square(r, FILE_F) : square(r, FILE_D);

				clear(us, KING, m.tsq);
				set(us, KING, ksq);
				set(us, ROOK, rsq);
			}
		}
	}

	turn = them;
	key ^= zobrist::turn();
	key ^= zobrist::ep(before.ep_square()) ^ zobrist::ep(epSquare);
	key ^= zobrist::castling(before.castlableRooks ^ castlableRooks);
	assert(key_ok());
}

void Position::print() const
{
	for (int r = RANK_8; r >= RANK_1; r--) {
		char line[] = ". . . . . . . .";
		for (int f = FILE_A; f <= FILE_H; f++) {
			int sq = square(r, f);
			line[2 * f] = bb::test(occupied(), sq)
				? PieceLabel[color_on(sq)][piece_on(sq)]
				: sq == epSquare ? '*' : '.';
		}
		std::cout << line << '\n';
	}
	std::cout << "\nrule50 = " << rule50 << std::endl;
}
