#include "types.h"

class Board {
	bitboard_t occ;
	bitboard_t byColor[NB_COLOR];
	bitboard_t byPiece[NB_PIECE];
	uint64_t key;
	int turn;

	void set(int color, int piece, int sq);
	void clear(int color, int piece, int sq);

	bool key_ok() const;

public:
	bitboard_t get(int color, int piece) const;
	bitboard_t get_RQ(int color) const;
	bitboard_t get_BQ(int color) const;

	int color_on(int sq) const;
	int piece_on(int sq) const;

	void print() const;
};
