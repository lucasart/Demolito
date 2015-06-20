#pragma once
#include "types.h"
#include "move.h"

class Position {
	bitboard_t _byColor[NB_COLOR];
	bitboard_t _byPiece[NB_PIECE];
	bitboard_t _castlableRooks;
	mutable bitboard_t _attacked, _checkers;	// lazy calc
	uint64_t _key;
	eval_t _pst;
	char _pieceOn[NB_SQUARE];
	int _turn, _epSquare, _rule50;
	eval_t _pieceMaterial[NB_COLOR];

	bool key_ok() const;
	bool pst_ok() const;
	bool material_ok() const;
	bitboard_t attacked_by(int color) const;

	void clear();
	void clear(int color, int piece, int sq);
	void set(int color, int piece, int sq);

public:
	void set_pos(const std::string& fen);
	std::string get_pos() const;
	void play(const Position& before, Move m);

	bitboard_t occ() const;
	bitboard_t occ(int color) const;
	bitboard_t by_piece(int piece) const;
	bitboard_t occ(int color, int piece) const;
	bitboard_t occ_RQ(int color) const;
	bitboard_t occ_BQ(int color) const;

	int turn() const;
	int ep_square() const;
	bitboard_t ep_square_bb() const;
	int rule50() const;
	bitboard_t checkers() const;
	bitboard_t attacked() const;
	bitboard_t castlable_rooks() const;
	uint64_t key() const;

	eval_t pst() const;
	eval_t piece_material() const;
	eval_t piece_material(int color) const;

	int king_square(int color) const;
	int color_on(int sq) const;
	int piece_on(int sq) const;

	bitboard_t attackers_to(int sq, bitboard_t _occ) const;

	void print() const;
};
