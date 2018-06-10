#pragma once
#include "position.h"
#include "types.h"

enum {MAX_MOVES = 192};

move_t *gen_pawn_moves(const Position *pos, move_t *emList, bitboard_t targets, bool subPromotions);
move_t *gen_piece_moves(const Position *pos, move_t *emList, bitboard_t targets, bool kingMoves);
move_t *gen_castling_moves(const Position *pos, move_t *emList);
move_t *gen_check_escapes(const Position *pos, move_t *emList, bool subPromotions);

move_t *gen_all_moves(const Position *pos, move_t *emList);
uint64_t gen_perft(const Position *pos, int depth, int ply);
