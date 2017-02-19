#pragma once
#include "types.h"
#include "move.h"

struct Position {
    bitboard_t byColor[NB_COLOR];
    bitboard_t byPiece[NB_PIECE];
    Color turn;
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
void pos_move(Position *pos, const Position& before, Move m);
void pos_switch(Position *pos, const Position& before);

bitboard_t attacked_by(const Position& pos, Color c);
bitboard_t calc_pins(const Position& pos);

uint64_t calc_key(const Position& pos);
uint64_t calc_pawn_key(const Position& pos);
eval_t calc_pst(const Position& pos);
eval_t calc_piece_material(const Position& pos, Color c);

bitboard_t pieces(const Position& pos);
bitboard_t pieces_cp(const Position& pos, Color c, int p);
bitboard_t pieces_cpp(const Position& pos, Color c, int p1, int p2);

std::string get(const Position& pos);
bitboard_t ep_square_bb(const Position& pos);
bool insufficient_material(const Position& pos);
int king_square(const Position& pos, Color c);
Color color_on(const Position& pos, int s);
bitboard_t attackers_to(const Position& pos, int s, bitboard_t occ);
void print(const Position& pos);
