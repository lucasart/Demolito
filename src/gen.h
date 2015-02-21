#pragma once
#include "position.h"
#include "move.h"

namespace gen {

Move *pawn_moves(const Position& pos, const PinInfo& pi, Move *mlist, bitboard_t targets);
Move *piece_moves(const Position& pos, const PinInfo& pi, Move *mlist, bitboard_t targets);
Move *castling_moves(const Position& pos, const PinInfo& pi, Move *mlist);

Move *all_moves(const Position& pos, const PinInfo& pi, Move *mlist);

// TODO (lucas#2#): check_evasions()

}	// namespace gen
