#pragma once
#include "position.h"
#include "move.h"

#define MAX_MOVES 192

namespace gen {

Move *pawn_moves(const Position& pos, Move *mList, bitboard_t targets);
Move *piece_moves(const Position& pos, Move *mList, bitboard_t targets, bool kingMoves = true);
Move *castling_moves(const Position& pos, Move *mList);
Move *check_escapes(const Position& pos, Move *mList);

Move *all_moves(const Position& pos, Move *mList);

template <bool Root> uint64_t perft(const Position& pos, int depth);

}	// namespace gen
