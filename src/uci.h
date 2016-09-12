#pragma once
#include <vector>
#include <mutex>
#include "move.h"

namespace uci {

void loop();

class Info {
public:
    Clock clock;

    Info() : lastDepth(0), best(0), ponder(0) { clock.reset(); }
    int last_depth() const { return lastDepth; }
    void update(const Position& pos, int depth, int score, int nodes, std::vector<move_t>& pv);
    void print_bestmove(const Position& pos) const;

private:
    int lastDepth;
    Move best, ponder;
    mutable std::mutex mtx;
};

std::string format_score(int score);

}    // namespace UCI
