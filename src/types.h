#pragma once
#include <string>
#include <cstdint>

typedef uint64_t bitboard_t;
typedef uint16_t move_t;	// encoded move: fsq:6, tsq:6, prom: 3 (NB_PIECE if none)

struct Move {			// decoded move
	int fsq, tsq, prom;

	bool ok() const;
	move_t encode() const;
	void decode(move_t em);
};

extern bool Chess960;

/* Color, Piece */

enum {WHITE, BLACK, NB_COLOR};
enum {KNIGHT, BISHOP, ROOK, QUEEN, KING, PAWN, NB_PIECE};

bool color_ok(int color);
bool piece_ok(int piece);
int opp_color(int color);

/* Rank, File, Square */

enum {RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, NB_RANK};
enum {FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, NB_FILE};
enum {
	A1, B1, C1, D1, E1, F1, G1, H1,
	A2, B2, C2, D2, E2, F2, G2, H2,
	A3, B3, C3, D3, E3, F3, G3, H3,
	A4, B4, C4, D4, E4, F4, G4, H4,
	A5, B5, C5, D5, E5, F5, G5, H5,
	A6, B6, C6, D6, E6, F6, G6, H6,
	A7, B7, C7, D7, E7, F7, G7, H7,
	A8, B8, C8, D8, E8, F8, G8, H8,
	NB_SQUARE
};

bool rank_ok(int r);
bool file_ok(int f);
bool square_ok(int sq);
int rank_of(int sq);
int file_of(int sq);
int square(int r, int f);

std::string square_to_string(int sq);

/* Directions */

int push_inc(int color);	// pawn push increment

/* Display */

extern const std::string PieceLabel[NB_COLOR];
