#include "types.h"

class Board {
	bitboard_t byColor[NB_COLOR];
	bitboard_t byPiece[NB_PIECE];
	bitboard_t castlableRooks;
	uint64_t key;
	int turn;

	bitboard_t occupied() const;
	bool key_ok() const;

	void clear();
	void clear(int color, int piece, int sq);
	void set(int color, int piece, int sq);

public:
	void set_pos(const std::string& fen);
	std::string get_pos() const;

	bitboard_t get(int color, int piece) const;
	bitboard_t get_RQ(int color) const;
	bitboard_t get_BQ(int color) const;

	int color_on(int sq) const;
	int piece_on(int sq) const;

	void print() const;
};
