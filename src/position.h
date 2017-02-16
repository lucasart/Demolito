#pragma once
#include "types.h"
#include "move.h"

class Position {
    void clear();
    void clear(Color c, Piece p, Square s);
    void set(Color c, Piece p, Square s);
    void finish();

public:
    bitboard_t byColor[NB_COLOR];
    bitboard_t byPiece[NB_PIECE];
    Color turn;
    bitboard_t castleRooks;
    Square epSquare;
    int rule50;

    bitboard_t attacked, checkers, pins;
    uint64_t key, pawnKey;
    eval_t pst;
    char pieceOn[NB_SQUARE];
    eval_t pieceMaterial[NB_COLOR];

    void set(const std::string& fen);
    void set(const Position& before, Move m);
    void toggle(const Position& before);
};

bitboard_t attacked_by(const Position& pos, Color c);
bitboard_t calc_pins(const Position& pos);

uint64_t calc_key(const Position& pos);
uint64_t calc_pawn_key(const Position& pos);
eval_t calc_pst(const Position& pos);
eval_t calc_piece_material(const Position& pos, Color c);

bitboard_t pieces(const Position& pos);
bitboard_t pieces_cp(const Position& pos, Color c, Piece p);
bitboard_t pieces_cpp(const Position& pos, Color c, Piece p1, Piece p2);

std::string get(const Position& pos);
bitboard_t ep_square_bb(const Position& pos);
bool insufficient_material(const Position& pos);
Square king_square(const Position& pos, Color c);
Color color_on(const Position& pos, Square s);
bitboard_t attackers_to(const Position& pos, Square s, bitboard_t occ);
void print(const Position& pos);
