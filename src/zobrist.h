#pragma once
#include "types.h"

namespace zobrist {

void init();

uint64_t key(int color, int piece, int sq);
uint64_t castling(bitboard_t castlable_rooks);
uint64_t ep(int sq);
uint64_t turn();

}	// namespace zobrist
