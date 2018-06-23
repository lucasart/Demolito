#pragma once
#include <stdatomic.h>
#include "position.h"

enum {
    INF = 32767, MATE = 32000,
    MAX_DEPTH = 127, MIN_DEPTH = -8,
    MAX_PLY = MAX_DEPTH - MIN_DEPTH + 2,
    MAX_GAME_PLY = 2048
};

typedef struct {
    uint64_t keys[MAX_GAME_PLY];
    int idx;
} Stack;

typedef struct {
    int depth, movestogo;
    int64_t movetime, time, inc, nodes;
} Limits;

int mated_in(int ply);
int mate_in(int ply);
bool is_mate_score(int score);

extern atomic_uint_fast64_t Signal;
enum {STOP = (uint_fast64_t)(-1)};

extern Position rootPos;
extern Stack rootStack;
extern Limits lim;
extern int Contempt;

void search_init();
int64_t search_go();
