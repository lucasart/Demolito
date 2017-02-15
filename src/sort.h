#pragma once
#include "gen.h"

namespace search {

class History {
public:
    void clear();
    int get(Move m) const;
    void update(Move m, int bonus);

    static const int Max = 2000;
private:
    int table[NB_SQUARE][NB_SQUARE];
};

extern thread_local History H;

class Selector {
public:
    Selector(const Position& pos, int depth, move_t ttMove);
    Move select(const Position& pos, int& see);
    bool done() const { return idx == cnt; }

    move_t moves[MAX_MOVES];
    int scores[MAX_MOVES];
    size_t cnt, idx;

private:
    void generate(const Position& pos, int depth);
    void score(const Position& pos, move_t ttMove);
};

}    // namespace search
