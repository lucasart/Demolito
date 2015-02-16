#pragma once
#include "bitboard.h"
#include "types.h"

namespace zobrist {

class PRNG {
	uint64_t a, b, c, d;
public:
	PRNG() { init(); }
	void init(uint64_t seed = 0);
	uint64_t rand();
};

void init();

uint64_t key(int color, int piece, int sq);
uint64_t castling(bitboard_t castlableRooks);
uint64_t en_passant(int sq);
uint64_t turn();

}	// namespace zobrist
