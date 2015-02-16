#pragma once
#include "position.h"
#include "move.h"

namespace gen {

Move *piece_moves(const Position& pos, Move *mlist, bitboard_t targets);

}	// namespace gen
