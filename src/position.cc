/*
 * Demolito, a UCI chess engine.
 * Copyright 2015 lucasart.
 *
 * Demolito is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Demolito is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If
 * not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include <sstream>
#include <string.h>    // memset
#include "bitboard.h"
#include "position.h"
#include "pst.h"
#include "zobrist.h"

static void clear(Position *pos)
{
    memset(pos, 0, sizeof(*pos));

    for (int s = A1; s <= H8; ++s)
        pos->pieceOn[s] = NB_PIECE;
}

static void clear_square(Position *pos, int c, int p, int s)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p, NB_PIECE);
    BOUNDS(s, NB_SQUARE);

    bb_clear(&pos->byColor[c], s);
    bb_clear(&pos->byPiece[p], s);

    pos->pieceOn[s] = NB_PIECE;
    pos->pst -= pst::table[c][p][s];
    pos->key ^= zobrist::key(c, p, s);

    if (p <= QUEEN)
        pos->pieceMaterial[c] -= Material[p];
    else
        pos->pawnKey ^= zobrist::key(c, p, s);
}

static void set_square(Position *pos, int c, int p, int s)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p, NB_PIECE);
    BOUNDS(s, NB_SQUARE);

    bb_set(&pos->byColor[c], s);
    bb_set(&pos->byPiece[p], s);

    pos->pieceOn[s] = p;
    pos->pst += pst::table[c][p][s];
    pos->key ^= zobrist::key(c, p, s);

    if (p <= QUEEN)
        pos->pieceMaterial[c] += Material[p];
    else
        pos->pawnKey ^= zobrist::key(c, p, s);
}

static void finish(Position *pos)
{
    const int us = pos->turn, them = opposite(us);
    const int ksq = king_square(*pos, us);

    pos->attacked = attacked_by(*pos, them);
    pos->checkers = bb_test(pos->attacked, ksq) ? attackers_to(*pos, ksq,
                    pieces(*pos)) & pos->byColor[them] : 0;
    pos->pins = calc_pins(*pos);
}

void pos_set(Position *pos, const std::string& fen)
{
    clear(pos);
    std::istringstream is(fen);
    std::string token;

    // int placement
    is >> token;
    int s = A8;

    for (char c : token) {
        if (isdigit(c))
            s += c - '0';
        else if (c == '/')
            s += 2 * DOWN;
        else {
            for (int col = WHITE; col <= BLACK; ++col) {
                const int p = PieceLabel[col].find(c);

                if (unsigned(p) < NB_PIECE) {
                    set_square(pos, col, p, s);
                    ++s;
                }
            }
        }
    }

    // Turn of play
    is >> token;

    if (token == "w")
        pos->turn = WHITE;
    else {
        pos->turn = BLACK;
        pos->key ^= zobrist::turn();
    }

    // Castling rights
    is >> token;

    if (token != "-") {
        for (char c : token) {
            const int r = isupper(c) ? RANK_1 : RANK_8;
            c = toupper(c);

            if (c == 'K')
                s = square(r, FILE_H);
            else if (c == 'Q')
                s = square(r, FILE_A);
            else if ('A' <= c && c <= 'H')
                s = square(r, c - 'A');

            bb_set(&pos->castleRooks, s);
        }

        pos->key ^= zobrist::castling(pos->castleRooks);
    }

    // En passant and 50 move
    is >> token;
    pos->epSquare = string_to_square(token);
    pos->key ^= zobrist::en_passant(pos->epSquare);
    is >> pos->rule50;

    finish(pos);
}

void pos_move(Position *pos, const Position& before, Move m)
{
    *pos = before;
    pos->rule50++;

    const int us = pos->turn, them = opposite(us);
    const int p = pos->pieceOn[m.from];
    const int capture = pos->pieceOn[m.to];

    // Capture piece on to square (if any)
    if (capture != NB_PIECE) {
        pos->rule50 = 0;
        // Use color_on() instead of them, because we could be playing a KxR castling here
        clear_square(pos, color_on(*pos, m.to), capture, m.to);

        // Capturing a rook alters corresponding castling right
        if (capture == ROOK)
            pos->castleRooks &= ~(1ULL << m.to);
    }

    // Move our piece
    clear_square(pos, us, p, m.from);
    set_square(pos, us, p, m.to);

    if (p == PAWN) {
        // reset rule50, and set epSquare
        const int push = push_inc(us);
        pos->rule50 = 0;
        pos->epSquare = m.to == m.from + 2 * push ? m.from + push : NB_SQUARE;

        // handle ep-capture and promotion
        if (m.to == before.epSquare)
            clear_square(pos, them, p, m.to - push);
        else if (rank_of(m.to) == RANK_8 || rank_of(m.to) == RANK_1) {
            clear_square(pos, us, p, m.to);
            set_square(pos, us, m.prom, m.to);
        }
    } else {
        pos->epSquare = NB_SQUARE;

        if (p == ROOK)
            // remove corresponding castling right
            pos->castleRooks &= ~(1ULL << m.from);
        else if (p == KING) {
            // Lose all castling rights
            pos->castleRooks &= ~bb_rank(us * RANK_8);

            // Castling
            if (bb_test(before.byColor[us], m.to)) {
                // Capturing our own piece can only be a castling move, encoded KxR
                assert(before.pieceOn[m.to] == ROOK);
                const int r = rank_of(m.from);

                clear_square(pos, us, KING, m.to);
                set_square(pos, us, KING, square(r, m.to > m.from ? FILE_G : FILE_C));
                set_square(pos, us, ROOK, square(r, m.to > m.from ? FILE_F : FILE_D));
            }
        }
    }

    pos->turn = them;
    pos->key ^= zobrist::turn();
    pos->key ^= zobrist::en_passant(before.epSquare) ^ zobrist::en_passant(pos->epSquare);
    pos->key ^= zobrist::castling(before.castleRooks ^ pos->castleRooks);

    finish(pos);
}

void pos_switch(Position *pos, const Position& before)
{
    *pos = before;
    pos->epSquare = NB_SQUARE;

    pos->turn = opposite(pos->turn);
    pos->key ^= zobrist::turn();
    pos->key ^= zobrist::en_passant(before.epSquare) ^ zobrist::en_passant(pos->epSquare);

    finish(pos);
}

bitboard_t attacked_by(const Position& pos, int c)
{
    BOUNDS(c, NB_COLOR);

    // King and Knight attacks
    bitboard_t result = bb_kattacks(king_square(pos, c));
    bitboard_t fss = pieces_cp(pos, c, KNIGHT);

    while (fss)
        result |= bb_nattacks(bb_pop_lsb(&fss));

    // Pawn captures
    fss = pieces_cp(pos, c, PAWN) & ~bb_file(FILE_A);
    result |= bb_shift(fss, push_inc(c) + LEFT);
    fss = pieces_cp(pos, c, PAWN) & ~bb_file(FILE_H);
    result |= bb_shift(fss, push_inc(c) + RIGHT);

    // Sliders
    bitboard_t _occ = pieces(pos) ^ pieces_cp(pos, opposite(c), KING);
    fss = pieces_cpp(pos, c, ROOK, QUEEN);

    while (fss)
        result |= bb_rattacks(bb_pop_lsb(&fss), _occ);

    fss = pieces_cpp(pos, c, BISHOP, QUEEN);

    while (fss)
        result |= bb_battacks(bb_pop_lsb(&fss), _occ);

    return result;
}

bitboard_t calc_pins(const Position& pos)
{
    const int us = pos.turn, them = opposite(us);
    const int king = king_square(pos, us);
    bitboard_t pinners = (pieces_cpp(pos, them, ROOK, QUEEN) & bb_rpattacks(king))
                         | (pieces_cpp(pos, them, BISHOP, QUEEN) & bb_bpattacks(king));
    bitboard_t result = 0;

    while (pinners) {
        const int s = bb_pop_lsb(&pinners);
        bitboard_t skewered = bb_segment(king, s) & pieces(pos);
        bb_clear(&skewered, king);
        bb_clear(&skewered, s);

        if (!bb_several(skewered) && (skewered & pos.byColor[us]))
            result |= skewered;
    }

    return result;
}

uint64_t calc_key(const Position& pos)
{
    uint64_t key = (pos.turn ? zobrist::turn() : 0)
                   ^ zobrist::en_passant(pos.epSquare)
                   ^ zobrist::castling(pos.castleRooks);

    for (int c = WHITE; c <= BLACK; ++c)
        for (int p = KNIGHT; p < NB_PIECE; ++p)
            key ^= zobrist::keys(c, p, pieces_cp(pos, c, p));

    return key;
}

uint64_t calc_pawn_key(const Position& pos)
{
    uint64_t key = 0;

    for (int c = WHITE; c <= BLACK; ++c) {
        key ^= zobrist::keys(c, PAWN, pieces_cp(pos, c, PAWN));
        key ^= zobrist::keys(c, KING, pieces_cp(pos, c, KING));
    }

    return key;
}

eval_t calc_pst(const Position& pos)
{
    eval_t result {0, 0};

    for (int c = WHITE; c <= BLACK; ++c)
        for (int p = KNIGHT; p < NB_PIECE; ++p) {
            bitboard_t b = pieces_cp(pos, c, p);

            while (b)
                result += pst::table[c][p][bb_pop_lsb(&b)];
        }

    return result;
}

eval_t calc_piece_material(const Position& pos, int c)
{
    eval_t result {0, 0};

    for (int p = KNIGHT; p <= QUEEN; ++p)
        result += Material[p] * bb_count(pieces_cp(pos, c, p));

    return result;
}

bitboard_t pieces_cp(const Position& pos, int c, int p)
{
    return pos.byColor[c] & pos.byPiece[p];
}

bitboard_t pieces(const Position& pos)
{
    assert(!(pos.byColor[WHITE] & pos.byColor[BLACK]));

    return pos.byColor[WHITE] | pos.byColor[BLACK];
}

bitboard_t pieces_cpp(const Position& pos, int c, int p1, int p2)
{
    return pos.byColor[c] & (pos.byPiece[p1] | pos.byPiece[p2]);
}

std::string get(const Position& pos)
{
    std::ostringstream os;

    // int placement
    for (int r = RANK_8; r >= RANK_1; --r) {
        int cnt = 0;

        for (int f = FILE_A; f <= FILE_H; ++f) {
            const int s = square(r, f);

            if (bb_test(pieces(pos), s)) {
                if (cnt)
                    os << char(cnt + '0');

                os << PieceLabel[color_on(pos, s)][pos.pieceOn[s]];
                cnt = 0;
            } else
                cnt++;
        }

        if (cnt)
            os << char(cnt + '0');

        os << (r == RANK_1 ? ' ' : '/');
    }

    // Turn of play
    os << (pos.turn == WHITE ? "w " : "b ");

    // Castling rights
    if (!pos.castleRooks)
        os << '-';
    else {
        for (int c = WHITE; c <= BLACK; ++c) {
            const bitboard_t sqs = pos.castleRooks & pos.byColor[c];

            if (!sqs)
                continue;

            const int king = king_square(pos, c);

            // Because we have castlable rooks, the king has to be on the first rank and
            // cannot be in a corner, which allows using bb_ray(king, king +/- 1) to
            // search for the castle rook in Chess960.
            assert(rank_of(king) == relative_rank(c, RANK_1));
            assert(file_of(king) != FILE_A && file_of(king) != FILE_H);

            // Right side castling
            if (sqs & bb_ray(king, king + 1)) {
                if (Chess960)
                    os << char(file_of(bb_lsb(sqs & bb_ray(king, king + 1)))
                               + (c == WHITE ? 'A' : 'a'));
                else
                    os << PieceLabel[c][KING];
            }

            // Left side castling
            if (sqs & bb_ray(king, king - 1)) {
                if (Chess960)
                    os << char(file_of(bb_msb(sqs & bb_ray(king, king - 1)))
                               + (c == WHITE ? 'A' : 'a'));
                else
                    os << PieceLabel[c][QUEEN];
            }
        }
    }

    os << ' ';

    // En passant and 50 move
    os << (pos.epSquare < NB_SQUARE ? square_to_string(pos.epSquare) : "-") << ' ';
    os << pos.rule50;

    return os.str();
}

bitboard_t ep_square_bb(const Position& pos)
{
    return pos.epSquare < NB_SQUARE ? 1ULL << pos.epSquare : 0;
}

bool insufficient_material(const Position& pos)
{
    return bb_count(pieces(pos)) <= 3 && !pos.byPiece[PAWN] && !pos.byPiece[ROOK]
           && !pos.byPiece[QUEEN];
}

int king_square(const Position& pos, int c)
{
    assert(bb_count(pieces_cp(pos, c, KING)) == 1);

    return bb_lsb(pieces_cp(pos, c, KING));
}

int color_on(const Position& pos, int s)
{
    assert(bb_test(pieces(pos), s));

    return bb_test(pos.byColor[WHITE], s) ? WHITE : BLACK;
}

bitboard_t attackers_to(const Position& pos, int s, bitboard_t occ)
{
    BOUNDS(s, NB_SQUARE);

    return (pieces_cp(pos, WHITE, PAWN) & bb_pattacks(BLACK, s))
           | (pieces_cp(pos, BLACK, PAWN) & bb_pattacks(WHITE, s))
           | (bb_nattacks(s) & pos.byPiece[KNIGHT])
           | (bb_kattacks(s) & pos.byPiece[KING])
           | (bb_rattacks(s, occ) & (pos.byPiece[ROOK] | pos.byPiece[QUEEN]))
           | (bb_battacks(s, occ) & (pos.byPiece[BISHOP] | pos.byPiece[QUEEN]));
}

void print(const Position& pos)
{
    for (int r = RANK_8; r >= RANK_1; --r) {
        char line[] = ". . . . . . . .";

        for (int f = FILE_A; f <= FILE_H; ++f) {
            const int s = square(r, f);
            line[2 * f] = bb_test(pieces(pos), s)
                          ? PieceLabel[color_on(pos, s)][pos.pieceOn[s]]
                          : s == pos.epSquare ? '*' : '.';
        }

        std::cout << line << '\n';
    }

    std::cout << get(pos) << std::endl;

    bitboard_t b = pos.checkers;

    if (b) {
        std::cout << "checkers:";

        while (b)
            std::cout << ' ' << square_to_string(bb_pop_lsb(&b));

        std::cout << std::endl;
    }
}
