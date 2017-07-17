#pragma once
#include "types.h"

uint64_t prng(uint64_t *x);

typedef struct {
    uint64_t keys[MAX_GAME_PLY];
    int idx;
} Stack;

void stack_clear(Stack *gs);
void stack_push(Stack *gs, uint64_t key);
void stack_pop(Stack *gs);
uint64_t stack_back(const Stack *gs);
uint64_t stack_move_key(const Stack *gs);
bool stack_repetition(const Stack *gs, int rule50);

extern uint64_t ZobristKey[NB_COLOR][NB_PIECE][NB_SQUARE];
extern uint64_t ZobristEnPassant[NB_SQUARE + 1];
extern uint64_t ZobristTurn;

void zobrist_init();
uint64_t zobrist_keys(int c, int p, uint64_t sqs);
uint64_t zobrist_castling(bitboard_t castlableRooks);
