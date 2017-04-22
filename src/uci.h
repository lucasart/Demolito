#pragma once
#include "threads.h"
#include "move.h"

void uci_loop();

struct Info {
    struct timespec start;

    int lastDepth;
    move_t best, ponder;
    mtx_t mtx;
};

extern Info ui;

void info_create(Info *info);
void info_destroy(Info *info);

void info_update(Info *info, int depth, int score, int64_t nodes, move_t pv[],
                 bool partial = false);
void info_print_bestmove(Info *info);
move_t info_best(Info *info);
int info_last_depth(Info *info);
