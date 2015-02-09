#include "types.h"

class Position {
	bitboard_t byColor[NB_COLOR];	// occupied squares by color
	bitboard_t byPiece[NB_PIECE];	// occupied squares by piece
	bitboard_t castlableRooks;	// rooks that can castle (both colors)
	uint64_t key;			// zobrist key of the position
	int turn;			// turn of play (WHITE or BLACK)
	int epSquare;			// en-passant square (0 if none)
	int rule50;			// half-move counter for the 50 move rule

	bitboard_t occupied() const;
	bool key_ok() const;

	void clear();					// clear everything
	void clear(int color, int piece, int sq);	// remove piece from color on sq
	void set(int color, int piece, int sq);		// set piece of color on sq

public:
	void set_pos(const std::string& fen);		// set position from fen

	bitboard_t get(int color, int piece) const;	// pieces of a given color and type
	bitboard_t get_RQ(int color) const;		// rooks and queens of color
	bitboard_t get_BQ(int color) const;		// bishops and queens of color

	int color_on(int sq) const;	// color of piece on square (assumed occupied)
	int piece_on(int sq) const;	// type of piece on square (assumed occupied)

	void print() const;	// ASCII representation of the position
};
