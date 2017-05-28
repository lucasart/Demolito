#pragma once
#include "types.h"

typedef uint16_t move_t;  // from:6, to:6, prom: 3 (NB_PIECE if none)

bool move_ok(move_t m);

int move_from(move_t m);
int move_to(move_t m);
int move_prom(move_t m);
move_t move_build(int from, int to, int prom);

void move_to_string(const Position *pos, move_t m, char *str);
move_t string_to_move(const Position *pos, const char *str);

bool move_is_capture(const Position *pos, move_t m);
bool move_is_castling(const Position *pos, move_t m);

bool move_is_legal(const Position *pos, move_t m);
int move_see(const Position *pos, move_t m);
