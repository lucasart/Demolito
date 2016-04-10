#pragma once
#include <vector>
#include <mutex>
#include "move.h"
#include "zobrist.h"

namespace uci {

extern zobrist::History history;

void loop();

class Info {
    int lastDepth;
    Move best, ponder;
    mutable std::mutex mtx;
public:
    Info() : lastDepth(0), best(0), ponder(0) {}
    int last_depth() const { return lastDepth; }
    void update(const Position& pos, int depth, int score, int nodes, std::vector<move_t>& pv);
    void print_bestmove(const Position& pos) const;
};

std::string format_score(int score);

}    // namespace UCI
