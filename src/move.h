#pragma once
#include "bitboard.h"
#include "types.h"

class Position;

struct PinInfo {
	bitboard_t pinned;		// pinned pieces
	bitboard_t discoCheckers;	// discovery checkers

	bitboard_t hidden_checkers(const Position& b, int attacker, int blocker) const;
	PinInfo(const Position& pos);
};

typedef uint16_t move_t;	// fsq:6, tsq:6, prom: 3 (NB_PIECE if none)

struct Move {
	int fsq, tsq, prom;

	bool ok() const;
	void clear();

	move_t encode() const;
	void decode(move_t em);

	bool is_tactical(const Position& pos) const;
	operator bool() const { return tsq + fsq; }

	std::string to_string() const;
	void from_string(const std::string& s);

	Move() = default;
	Move(const std::string& s) { from_string(s); }

	bool pseudo_is_legal(const Position& pos, const PinInfo& pi) const;
	int see(const Position& pos) const;
};
