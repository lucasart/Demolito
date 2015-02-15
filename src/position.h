#include "types.h"
#include "move.h"

class Position {
	bitboard_t byColor[NB_COLOR];	// occupied squares by color
	bitboard_t byPiece[NB_PIECE];	// occupied squares by piece
	bitboard_t castlableRooks;	// rooks that can castle (both colors)
	uint64_t key;			// zobrist key of the position
	int turn;			// turn of play (WHITE or BLACK)
	int epSquare;			// en-passant square (NB_SQUARE if none)
	int rule50;			// half-move counter for the 50 move rule

	bool key_ok() const;

	void clear();
	void clear(int color, int piece, int sq);
	void set(int color, int piece, int sq);

public:
	void set_pos(const std::string& fen);
	std::string get_pos() const;
	void play(const Position& before, Move m);

	bitboard_t occupied() const;
	bitboard_t occupied(int color) const;

	int get_turn() const;

	bitboard_t get(int color, int piece) const;
	bitboard_t get_RQ(int color) const;
	bitboard_t get_BQ(int color) const;

	int ep_square() const;
	int king_square(int color) const;

	int color_on(int sq) const;
	int piece_on(int sq) const;

	void print() const;
};
