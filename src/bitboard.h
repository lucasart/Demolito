#pragma once
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>

#define BOUNDS(v, ub) assert((unsigned)(v) < (ub))

typedef uint64_t bitboard_t;

enum {RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, NB_RANK};
enum {FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, NB_FILE};

enum {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NB_SQUARE
};

enum {UP = 8, DOWN = -8, LEFT = -1, RIGHT = 1};

enum {WHITE, BLACK, NB_COLOR};
enum {KNIGHT, BISHOP, ROOK, QUEEN, KING, PAWN, NB_PIECE};

extern int64_t dbgCnt[2];

int opposite(int c);
int push_inc(int c);

int square(int r, int f);
int rank_of(int s);
int file_of(int s);

int relative_rank(int c, int r);
int relative_rank_of(int c, int s);

void bb_init();

extern bitboard_t Rank[NB_RANK], File[NB_FILE];
extern bitboard_t PawnAttacks[NB_COLOR][NB_SQUARE], KnightAttacks[NB_SQUARE], KingAttacks[NB_SQUARE];
extern bitboard_t BishopPseudoAttacks[NB_SQUARE], RookPseudoAttacks[NB_SQUARE];
extern bitboard_t Segment[NB_SQUARE][NB_SQUARE];
extern bitboard_t Ray[NB_SQUARE][NB_SQUARE];

bitboard_t bb_bishop_attacks(int s, bitboard_t occ);
bitboard_t bb_rook_attacks(int s, bitboard_t occ);

bool bb_test(bitboard_t b, int s);
void bb_clear(bitboard_t *b, int s);
void bb_set(bitboard_t *b, int s);
bitboard_t bb_shift(bitboard_t b, int i);

int bb_lsb(bitboard_t b);
int bb_msb(bitboard_t b);
int bb_pop_lsb(bitboard_t *b);

bool bb_several(bitboard_t b);
int bb_count(bitboard_t b);

void bb_print(bitboard_t b);
