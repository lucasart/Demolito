#pragma once
#include <mutex>
#include "move.h"

void uci_loop();

struct Info {
    Clock clock;  // Read-only during search (doesn't need lock protection)

    int lastDepth;
    move_t best, ponder;
    mutable std::mutex mtx;
};

extern Info ui;

void info_clear(Info *info);
void info_update(Info *info, int depth, int score, uint64_t nodes, move_t pv[],
                 bool partial = false);
void info_print_bestmove(const Info *info);
move_t info_best(const Info *info);
int info_last_depth(const Info *info);

std::string uci_format_score(int score);
