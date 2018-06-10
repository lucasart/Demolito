#pragma once
#include <inttypes.h>
#include <stdbool.h>

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
#define swap(x, y) do { typeof(x) tmp = x; x = y; y = tmp; } while (0);

#define BOUNDS(v, ub) assert((unsigned)(v) < (ub))

extern int64_t dbgCnt[2];

// Color, Piece

enum {WHITE, BLACK, NB_COLOR};
enum {KNIGHT, BISHOP, ROOK, QUEEN, KING, PAWN, NB_PIECE};

int opposite(int c);
extern const char *PieceLabel[NB_COLOR];

// Rank, File

enum {RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, NB_RANK};
enum {FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, NB_FILE};

int rank_of(int s);
int file_of(int s);
int relative_rank(int c, int r);
int relative_rank_of(int c, int s);

// Square

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

int square(int r, int f);
void square_to_string(int s, char *str);
int string_to_square(const char *str);
int push_inc(int c);  // pawn push increment

// Eval

enum {
    OP = 158, EP = 200, P = (OP + EP) / 2,
    N = 640, B = 640,
    R = 1046, Q = 1980
};

enum {OPENING, ENDGAME, NB_PHASE};

typedef struct {
    int op, eg;
} eval_t;

void eval_add(eval_t *e1, eval_t e2);
void eval_sub(eval_t *e1, eval_t e2);

enum {
    INF = 32767, MATE = 32000,
    MAX_DEPTH = 127, MIN_DEPTH = -8,
    MAX_PLY = MAX_DEPTH - MIN_DEPTH + 2
};

bool is_mate_score(int score);
int mated_in(int ply);
int mate_in(int ply);

typedef uint16_t move_t;  // from:6, to:6, prom: 3 (NB_PIECE if none)
