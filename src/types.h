#pragma once
#include <cstdint>
#include <cassert>
#include <string>

extern bool Chess960;

#define ENABLE_OPERATORS(T, UB) \
inline T& operator++(T& v) { assert(0 <= v && v < UB); return v = T(v + 1); } \
inline T& operator--(T& v) { assert(0 <= v && v < UB); return v = T(v - 1); }

/* Color, Piece */

enum Color {WHITE, BLACK, NB_COLOR};
enum Piece {KNIGHT, BISHOP, ROOK, QUEEN, KING, PAWN, NB_PIECE};

ENABLE_OPERATORS(Color, NB_COLOR)
ENABLE_OPERATORS(Piece, NB_PIECE)

inline Color operator~(Color c) { return Color(c ^ BLACK); }

/* Rank, File, Square */

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

bool rank_ok(int r);
bool file_ok(int f);
bool square_ok(int sq);
int rank_of(int sq);
int file_of(int sq);
int relative_rank(Color c, int sq);
int square(int r, int f);

std::string square_to_string(int sq);
int string_to_square(const std::string& s);

/* Directions */

enum {UP = 8, DOWN = -8, LEFT = -1, RIGHT = 1};

int push_inc(Color c);    // pawn push increment

/* Material values */

enum {
    OP = 160, EP = 200, P = 180,
    N = 660, B = 660,
    R = 1080, Q = 2000
};

/* Eval */

enum {OPENING, ENDGAME, NB_PHASE};

/*#ifdef __clang__
typedef int eval_t __attribute__ (( ext_vector_type(2) ));
#else
typedef int eval_t __attribute__ (( vector_size(8) ));
#endif*/

struct eval_t {
    int v[NB_PHASE];

    int operator[](int phase) const { return v[phase]; }
    int op() const { return v[OPENING]; }
    int eg() const { return v[ENDGAME]; }
    int& op() { return v[OPENING]; }
    int& eg() { return v[ENDGAME]; }

    bool operator==(eval_t e) const { return op() == e.op() && eg() == e.eg(); }
    bool operator!=(eval_t e) const { return !(*this == e); }

    eval_t operator+(eval_t e) const { return {op() + e.op(), eg() + e.eg()}; }
    eval_t operator-(eval_t e) const { return {op() - e.op(), eg() - e.eg()}; }
    eval_t operator*(int x) const { return {op() * x, eg() * x}; }
    eval_t operator/(int x) const { return {op() / x, eg() / x}; }

    eval_t& operator+=(eval_t e) { return op() += e.op(), eg() += e.eg(), *this; }
    eval_t& operator-=(eval_t e) { return op() -= e.op(), eg() -= e.eg(), *this; }
    eval_t& operator*=(int x) { return op() *= x, eg() *= x, *this; }
    eval_t& operator/=(int x) { return op() /= x, eg() /= x, *this; }
};

extern const eval_t Material[NB_PIECE];

#define INF    32767
#define MATE    32000
#define MAX_DEPTH    127
#define MIN_DEPTH    -8
#define MAX_PLY        (MAX_DEPTH - MIN_DEPTH + 2)
#define MAX_GAME_PLY    1024

bool score_ok(int score);
bool is_mate_score(int score);
int mated_in(int ply);
int mate_in(int ply);

/* Display */

extern const std::string PieceLabel[NB_COLOR];
