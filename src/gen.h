#pragma once
#include "position.h"
#include "move.h"

#define MAX_MOVES 192

move_t *gen_pawn_moves(const Position& pos, move_t *emList, bitboard_t targets,
                       bool subPromotions = true);
move_t *gen_piece_moves(const Position& pos, move_t *emList, bitboard_t targets,
                        bool kingMoves = true);
move_t *gen_castling_moves(const Position& pos, move_t *emList);
move_t *gen_check_escapes(const Position& pos, move_t *emList, bool subPromotions = true);

move_t *gen_all_moves(const Position& pos, move_t *emList);

template <bool Root=true> uint64_t gen_perft(const Position& pos, int depth);
