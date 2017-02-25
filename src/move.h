#pragma once
#include "bitboard.h"
#include "types.h"

struct Position;

typedef uint16_t move_t;  // from:6, to:6, prom: 3 (NB_PIECE if none)

struct Move {
    int from, to;
    int prom;

    Move() = default;
    Move(move_t em) { *this = em; }

    operator move_t() const;
    Move operator =(move_t em);
};

bool move_ok(const Move *m);

std::string move_to_string(const Position *pos, const Move *m);
void move_from_string(const Position *pos, const std::string& s, Move *m);

bool move_is_capture(const Position *pos, const Move *m);
bool move_is_castling(const Position *pos, const Move *m);

bool move_is_legal(const Position *pos, const Move *m);
int move_see(const Position *pos, const Move *m);
