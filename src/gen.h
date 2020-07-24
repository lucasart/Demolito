/*
 * Demolito, a UCI chess engine. Copyright 2015-2020 lucasart.
 *
 * Demolito is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Demolito is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If
 * not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include "position.h"

// Max number of pseudo-legal moves allowed
enum {MAX_MOVES = 192};

// Generate pseudo-legal moves (ie. legal modulo self-check)
move_t *gen_pawn_moves(const Position *pos, move_t *mList, bitboard_t filter, bool subPromotions);
move_t *gen_piece_moves(const Position *pos, move_t *mList, bitboard_t filter, bool kingMoves);
move_t *gen_castling_moves(const Position *pos, move_t *mList);
move_t *gen_check_escapes(const Position *pos, move_t *mList, bool subPromotions);

// Verify legality of pseudo-legal moves generates by the above
bool gen_is_legal(const Position *pos, bitboard_t pins, move_t m);

// Count leaves of the full tree (ie. generate all legal moves at each node, no pruning)
uint64_t gen_perft(const Position *pos, int depth, int ply);
