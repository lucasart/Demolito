#pragma once
#include <vector>
#include <mutex>
#include "move.h"

namespace uci {

void loop();

struct Info {
    Clock clock;  // Read-only during search (doesn't need lock protection)

    int lastDepth;
    Move bestMove, ponderMove;
    mutable std::mutex mtx;
};

void info_clear(Info *info);
void info_update(Info *info, const Position *pos, int depth, int score, uint64_t nodes,
                 move_t pv[], bool partial = false);
void info_print_bestmove(const Info *info, const Position *pos);
Move info_best_move(const Info *info);
int info_last_depth(const Info *info);

extern Info ui;

std::string format_score(int score);

}    // namespace UCI
