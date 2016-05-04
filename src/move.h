#pragma once
#include "bitboard.h"
#include "types.h"

class Position;

struct PinInfo {
    bitboard_t pinned;        // pinned pieces
    bitboard_t discoCheckers;    // discovery checkers

    bitboard_t hidden_checkers(const Position& b, Color attacker, Color blocker) const;
    PinInfo(const Position& pos);
};

typedef uint16_t move_t;    // fsq:6, tsq:6, prom: 3 (NB_PIECE if none)

struct Move {
    int fsq, tsq;
    Piece prom;

    bool ok() const;
    void clear();

    Move() = default;
    Move(move_t em) { *this = em; }
    Move(const Position& pos, const std::string& s) { from_string(pos, s); }

    operator move_t() const;
    Move operator =(move_t em);

    bool null() const { return (fsq | tsq | prom) == 0; }
    bool is_capture(const Position& pos) const;
    bool is_castling(const Position& pos) const;

    std::string to_string(const Position& pos) const;
    void from_string(const Position& pos, const std::string& s);

    bool pseudo_is_legal(const Position& pos, const PinInfo& pi) const;
    int see(const Position& pos) const;
};
