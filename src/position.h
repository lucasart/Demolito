#pragma once
#include "types.h"
#include "move.h"

class Position {
    bitboard_t _byColor[NB_COLOR];
    bitboard_t _byPiece[NB_PIECE];
    bitboard_t _castlableRooks;
    bitboard_t _attacked, _checkers;
    uint64_t _key, _pawnKey;
    eval_t _pst;
    char _pieceOn[NB_SQUARE];
    Color _turn;
    Square _epSquare;
    int _rule50;
    eval_t _pieceMaterial[NB_COLOR];

    // Sanity check (helpers for assert)
    bool key_ok() const;
    bool pawn_key_ok() const;
    bool pst_ok() const;
    bool material_ok() const;
    bool castlable_rooks_ok() const;

    bitboard_t attacked_by(Color c) const;

    void clear();
    void clear(Color c, Piece p, Square s);
    void set(Color c, Piece p, Square s);

public:
    void set(const std::string& fen);
    void finish();

    void set(const Position& before, Move m);
    void toggle(const Position& before);

    bitboard_t occ() const;
    bitboard_t occ(Color c) const;
    bitboard_t occ(Piece p) const;
    bitboard_t occ(Piece p1, Piece p2) const;
    bitboard_t occ(Color c, Piece p) const;
    bitboard_t occ(Color c, Piece p1, Piece p2) const;

    Color turn() const;
    Square ep_square() const;
    int rule50() const;
    bitboard_t checkers() const;
    bitboard_t attacked() const;
    bitboard_t castlable_rooks() const;
    uint64_t key() const;
    uint64_t pawn_key() const;

    eval_t pst() const;
    eval_t piece_material() const;
    eval_t piece_material(Color c) const;

    Piece piece_on(Square s) const;
};

std::string get(const Position& pos);
bitboard_t ep_square_bb(const Position& pos);
bool insufficient_material(const Position& pos);
Square king_square(const Position& pos, Color c);
Color color_on(const Position& pos, Square s);
bitboard_t attackers_to(const Position& pos, Square s, bitboard_t occ);
void print(const Position& pos);
