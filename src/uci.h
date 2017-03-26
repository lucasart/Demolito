#pragma once
#include <vector>
#include <mutex>
#include "move.h"

void uci_loop();

struct Info {
    Clock clock;  // Read-only during search (doesn't need lock protection)

    int lastDepth;
    Move bestMove, ponderMove;
    mutable std::mutex mtx;
};

extern Info ui;

void info_clear(Info *info);
void info_update(Info *info, int depth, int score, uint64_t nodes, move_t pv[],
                 bool partial = false);
void info_print_bestmove(const Info *info);
Move info_best_move(const Info *info);
int info_last_depth(const Info *info);

std::string uci_format_score(int score);
