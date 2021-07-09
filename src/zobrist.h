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

enum { MAX_GAME_PLY = 2048 };

typedef struct {
    uint64_t keys[MAX_GAME_PLY];
    int idx;
} ZobristStack;

extern uint64_t ZobristKey[NB_COLOR][NB_PIECE][NB_SQUARE];
extern uint64_t ZobristCastling[NB_SQUARE];
extern uint64_t ZobristEnPassant[NB_SQUARE + 1];
extern uint64_t ZobristTurn;

uint64_t zobrist_castling(bitboard_t castleRooks);

void zobrist_clear(ZobristStack *st);
void zobrist_push(ZobristStack *st, uint64_t key);
void zobrist_pop(ZobristStack *st);
uint64_t zobrist_back(const ZobristStack *st);
uint64_t zobrist_move_key(const ZobristStack *st, int back);
bool zobrist_repetition(const ZobristStack *st, const Position *pos);
