#pragma once
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

extern int64_t dbgCnt[2];

#define BOUNDS(v, ub) assert((unsigned)(v) < (ub))

enum {WHITE, BLACK, NB_COLOR};
enum {KNIGHT, BISHOP, ROOK, QUEEN, KING, PAWN, NB_PIECE};

inline int opposite(int c) { return c ^ BLACK; }

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

int rank_of(int s);
int file_of(int s);
int relative_rank(int c, int r);
int relative_rank_of(int c, int s);
int square(int r, int f);

void square_to_string(int s, char *str);
int string_to_square(const char *str);

/* Directions */

enum {UP = 8, DOWN = -8, LEFT = -1, RIGHT = 1};

int push_inc(int c);    // pawn push increment

/* Material values */

enum {
    OP = 158, EP = 200, P = (OP + EP) / 2,
    N = 640, B = 640,
    R = 1046, Q = 1980
};

/* Eval */

enum {OPENING, ENDGAME, NB_PHASE};

typedef struct {
    int op, eg;
} eval_t;

static inline void eval_add(eval_t *e1, eval_t e2) { e1->op += e2.op; e1->eg += e2.eg; }
static inline void eval_sub(eval_t *e1, eval_t e2) { e1->op -= e2.op; e1->eg -= e2.eg; }

extern const eval_t Material[NB_PIECE];

enum {
    INF = 32767, MATE = 32000,
    MAX_DEPTH = 127, MIN_DEPTH = -8,
    MAX_PLY = MAX_DEPTH - MIN_DEPTH + 2,
    MAX_GAME_PLY = 1024,
    MAX_FEN = 64 + 8 + 2 + 5 + 3 + 4 + 4 + 1
};

bool score_ok(int score);
bool is_mate_score(int score);
int mated_in(int ply);
int mate_in(int ply);

typedef uint64_t bitboard_t;
typedef uint16_t move_t;  // from:6, to:6, prom: 3 (NB_PIECE if none)

typedef struct {
    bitboard_t byColor[NB_COLOR];
    bitboard_t byPiece[NB_PIECE];
    int turn;
    bitboard_t castleRooks;
    int epSquare;
    int rule50;
    bool chess960;

    bitboard_t attacked, checkers, pins;
    uint64_t key, pawnKey;
    eval_t pst;
    char pieceOn[NB_SQUARE];
    eval_t pieceMaterial[NB_COLOR];
} Position;

extern const char *PieceLabel[NB_COLOR];
