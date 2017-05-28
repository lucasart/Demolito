#pragma once
#include "types.h"

void pos_set(Position *pos, const char *fen);
void pos_get(const Position *pos, char *fen);

void pos_move(Position *pos, const Position *before, move_t m);
void pos_switch(Position *pos, const Position *before);

bitboard_t pos_pieces(const Position* pos);
bitboard_t pos_pieces_cp(const Position *pos, int c, int p);
bitboard_t pos_pieces_cpp(const Position *pos, int c, int p1, int p2);

bitboard_t pos_ep_square_bb(const Position *pos);
bool pos_insufficient_material(const Position *pos);
int pos_king_square(const Position *pos, int c);
int pos_color_on(const Position *pos, int s);
bitboard_t pos_attackers_to(const Position *pos, int s, bitboard_t occ);
void pos_print(const Position *pos);
