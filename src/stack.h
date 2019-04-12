#pragma once
#include "position.h"

enum {MAX_GAME_PLY = 2048};

typedef struct {
    uint64_t keys[MAX_GAME_PLY];
    int idx;
} Stack;

void stack_clear(Stack *st);
void stack_push(Stack *st, uint64_t key);
void stack_pop(Stack *st);
uint64_t stack_back(const Stack *st);
uint64_t stack_move_key(const Stack *st, int back);
bool stack_repetition(const Stack *st, const Position *pos);
