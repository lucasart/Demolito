#pragma once
#include "types.h"
#include "move.h"

class Position {
	bitboard_t _byColor[NB_COLOR];
	bitboard_t _byPiece[NB_PIECE];
	bitboard_t _castlableRooks;
	bitboard_t _checkers;
	mutable bitboard_t _attacked;
	uint64_t _key;
	int _turn;
	int _epSquare;
	int _rule50;

	bool key_ok() const;
	bitboard_t attackers_to(int sq) const;
	bitboard_t attacked_by(int color) const;

	void clear();
	void clear(int color, int piece, int sq);
	void set(int color, int piece, int sq);

public:
	void set_pos(const std::string& fen);
	std::string get_pos() const;
	void play(const Position& before, Move m, bool givesCheck);

	bitboard_t occ() const				{ return _byColor[WHITE] | _byColor[BLACK]; }
	bitboard_t occ(int color) const;
	bitboard_t by_piece(int piece) const;
	bitboard_t occ(int color, int piece) const;
	bitboard_t occ_RQ(int color) const;
	bitboard_t occ_BQ(int color) const;

	int turn() const					{ return _turn; }
	int ep_square() const				{ return _epSquare; }
	bitboard_t ep_square_bb() const		{ return square_ok(_epSquare) ? 1ULL << _epSquare : 0; }
	int rule50() const					{ return _rule50; }
	bitboard_t checkers() const			{ return _checkers; }
	bitboard_t attacked() const;
	bitboard_t castlable_rooks() const	{ return _castlableRooks; }
	uint64_t key() const				{ return _key; }

	int king_square(int color) const;
	int color_on(int sq) const;
	int piece_on(int sq) const;

	void print() const;
};
