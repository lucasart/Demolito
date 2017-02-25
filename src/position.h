#pragma once
#include "types.h"
#include "move.h"

struct Position {
    bitboard_t byColor[NB_COLOR];
    bitboard_t byPiece[NB_PIECE];
    int turn;
    bitboard_t castleRooks;
    int epSquare;
    int rule50;

    bitboard_t attacked, checkers, pins;
    uint64_t key, pawnKey;
    eval_t pst;
    char pieceOn[NB_SQUARE];
    eval_t pieceMaterial[NB_COLOR];
};

void pos_set(Position *pos, const std::string& fen);
void pos_move(Position *pos, const Position *before, Move m);
void pos_switch(Position *pos, const Position *before);

bitboard_t pos_pieces(const Position* pos);
bitboard_t pos_pieces_cp(const Position *pos, int c, int p);
bitboard_t pos_pieces_cpp(const Position *pos, int c, int p1, int p2);

std::string pos_get_fen(const Position *pos);
bitboard_t pos_ep_square_bb(const Position *pos);
bool pos_insufficient_material(const Position *pos);
int pos_king_square(const Position *pos, int c);
int pos_color_on(const Position *pos, int s);
bitboard_t pos_attackers_to(const Position *pos, int s, bitboard_t occ);
void pos_print(const Position *pos);
