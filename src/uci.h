#pragma once
#include "platform.h"

void uci_loop();

typedef struct {
    int64_t start;
    int lastDepth;
    double variability;
    move_t best;
    mtx_t mtx;
} Info;

extern Info ui;
extern int64_t TimeBuffer;
extern bool uciChess960;

void info_create(Info *info);
void info_destroy(Info *info);

void info_update(Info *info, int depth, int score, int64_t nodes, move_t pv[], bool partial);
void info_print_bestmove(Info *info);
move_t info_best(Info *info);
int info_last_depth(Info *info);
double info_variability(Info *info);
