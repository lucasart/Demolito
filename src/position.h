#pragma once
#include "bitboard.h"
#include "pst.h"

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

enum {MAX_FEN = 64 + 8 + 2 + 5 + 3 + 4 + 4 + 1};

typedef uint16_t move_t;  // from:6, to:6, prom: 3 (NB_PIECE if none)

typedef struct {
    bitboard_t byColor[NB_COLOR];
    bitboard_t byPiece[NB_PIECE];
    bitboard_t castleRooks;
    bitboard_t attacked, checkers, pins;
    uint64_t key, pawnKey;
    eval_t pst;
    eval_t pieceMaterial[NB_COLOR];
    uint8_t pieceOn[NB_SQUARE];
    int turn;
    int epSquare;
    int rule50;
} Position;

extern const char *PieceLabel[NB_COLOR];

void square_to_string(int s, char *str);
int string_to_square(const char *str);

void pos_init();

void pos_set(Position *pos, const char *fen);
void pos_get(const Position *pos, char *fen);

void pos_move(Position *pos, const Position *before, move_t m);
void pos_switch(Position *pos, const Position *before);

bitboard_t pos_pieces(const Position* pos);
bitboard_t pos_pieces_cp(const Position *pos, int c, int p);
bitboard_t pos_pieces_cpp(const Position *pos, int c, int p1, int p2);

bitboard_t pos_ep_square_bb(const Position *pos);
bool pos_insufficient_material(const Position *pos);
int pos_king_square(const Position *pos, int c);
int pos_color_on(const Position *pos, int s);
int pos_piece_on(const Position *pos, int s);
bitboard_t pos_attackers_to(const Position *pos, int s, bitboard_t occ);
void pos_print(const Position *pos);
