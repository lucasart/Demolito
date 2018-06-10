#pragma once
#include "position.h"
#include "types.h"

bool move_ok(move_t m);

int move_from(move_t m);
int move_to(move_t m);
int move_from_to(move_t m);
int move_prom(move_t m);
move_t move_build(int from, int to, int prom);

void move_to_string(const Position *pos, move_t m, char *str);
move_t string_to_move(const Position *pos, const char *str);

bool move_is_capture(const Position *pos, move_t m);
bool move_is_castling(const Position *pos, move_t m);

bool move_is_legal(const Position *pos, move_t m);
int move_see(const Position *pos, move_t m);
