#include "types.h"
#include "move.h"

class Position {
	bitboard_t byColor[NB_COLOR];
	bitboard_t byPiece[NB_PIECE];
	bitboard_t castlableRooks;
	bitboard_t checkers;
	uint64_t key;
	int turn;
	int epSquare;
	int rule50;

	bool key_ok() const;
	bitboard_t calc_checkers() const;

	void clear();
	void clear(int color, int piece, int sq);
	void set(int color, int piece, int sq);

public:
	void set_pos(const std::string& fen);
	std::string get_pos() const;
	void play(const Position& before, Move m, bool givesCheck);

	bitboard_t get_all() const;
	bitboard_t get_all(int color) const;

	bitboard_t get(int color, int piece) const;
	bitboard_t get_RQ(int color) const;
	bitboard_t get_BQ(int color) const;

	int get_turn() const;
	int ep_square() const;
	int king_square(int color) const;

	int color_on(int sq) const;
	int piece_on(int sq) const;

	void print() const;
};
