#pragma once
#include <vector>
#include <mutex>
#include <atomic>
#include "move.h"

namespace uci {

void loop();

struct Info {
    // Do not need synchronization
    Clock clock;  // read-only during search
    std::atomic<int> lastDepth;

    // Require synchronization
    Move bestMove, ponderMove;
    mutable std::mutex mtx;
};

void info_clear(Info *info);
void info_update(Info *info, const Position *pos, int depth, int score, uint64_t nodes,
                 move_t pv[], bool partial = false);
void info_print_bestmove(const Info *info, const Position *pos);
Move info_best_move(const Info *info);

extern Info ui;

std::string format_score(int score);

}    // namespace UCI
