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
#include <cstring>    // std::memset
#include "bitboard.h"
#include "position.h"
#include "pst.h"
#include "zobrist.h"

bool Position::key_ok() const
{
    uint64_t k = turn() ? zobrist::turn() : 0;
    k ^= zobrist::en_passant(ep_square());
    k ^= zobrist::castling(castlable_rooks());

    for (Color c = WHITE; c <= BLACK; ++c)
        for (Piece p = KNIGHT; p < NB_PIECE; ++p)
            k ^= zobrist::keys(c, p, pieces(*this, c, p));

    return k == _key;
}

bool Position::pawn_key_ok() const
{
    uint64_t k = 0;

    for (Color c = WHITE; c <= BLACK; ++c) {
        k ^= zobrist::keys(c, PAWN, pieces(*this, c, PAWN));
        k ^= zobrist::keys(c, KING, pieces(*this, c, KING));
    }

    return k == _pawnKey;
}

bool Position::pst_ok() const
{
    eval_t sum = {0, 0};

    for (Color c = WHITE; c <= BLACK; ++c)
        for (Piece p = KNIGHT; p < NB_PIECE; ++p) {
            bitboard_t b = pieces(*this, c, p);

            while (b)
                sum += pst::table[c][p][bb::pop_lsb(b)];
        }

    return _pst == sum;
}

bool Position::castlable_rooks_ok() const
{
    const bitboard_t b[2] = {_castlableRooks & bb::rank(RANK_1), _castlableRooks & bb::rank(RANK_8)};

    if ((b[0] | b[1]) != _castlableRooks)
        return false;

    for (Color c = WHITE; c <= BLACK; ++c) {
        if (!b[c])
            continue;    // nothing to verify

        if (b[c] & ~pieces(*this, c, ROOK))
            return false;    // castlable rooks on RANK_1/8 must be white/black rooks

        const Square k = king_square(*this, c);

        if (rank_of(k) != relative_rank(c, RANK_1))
            return false;    // king must be on first rank

        // There can be at most one castlable rook on each side of the king
        if (file_of(k) > FILE_A && bb::several(_castlableRooks & bb::ray(k, k - 1)))
            return false;

        if (file_of(k) < FILE_H && bb::several(_castlableRooks & bb::ray(k, k + 1)))
            return false;
    }

    return true;
}

bool Position::material_ok() const
{
    eval_t npm[NB_COLOR] = {{0,0}, {0,0}};

    for (Color c = WHITE; c <= BLACK; ++c) {
        for (Piece p = KNIGHT; p <= QUEEN; ++p)
            npm[c] += Material[p] * bb::count(pieces(*this, c, p));

        if (npm[c] != _pieceMaterial[c])
            return false;
    }

    return true;
}

bitboard_t Position::attacked_by(Color c) const
{
    BOUNDS(c, NB_COLOR);

    bitboard_t fss, result;

    // King and Knight attacks
    result = bb::kattacks(king_square(*this, c));
    fss = pieces(*this, c, KNIGHT);

    while (fss)
        result |= bb::nattacks(bb::pop_lsb(fss));

    // Pawn captures
    fss = pieces(*this, c, PAWN) & ~bb::file(FILE_A);
    result |= bb::shift(fss, push_inc(c) + LEFT);
    fss = pieces(*this, c, PAWN) & ~bb::file(FILE_H);
    result |= bb::shift(fss, push_inc(c) + RIGHT);

    // Sliders
    bitboard_t _occ = pieces(*this) ^ pieces(*this, ~c, KING);
    fss = pieces(*this, c, ROOK, QUEEN);

    while (fss)
        result |= bb::rattacks(bb::pop_lsb(fss), _occ);

    fss = pieces(*this, c, BISHOP, QUEEN);

    while (fss)
        result |= bb::battacks(bb::pop_lsb(fss), _occ);

    return result;
}

void Position::clear()
{
    std::memset(this, 0, sizeof(*this));

    for (Square s = A1; s <= H8; ++s)
        _pieceOn[s] = NB_PIECE;
}

void Position::clear(Color c, Piece p, Square s)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p, NB_PIECE);
    BOUNDS(s, NB_SQUARE);

    bb::clear(_byColor[c], s);
    bb::clear(_byPiece[p], s);

    _pieceOn[s] = NB_PIECE;
    _pst -= pst::table[c][p][s];
    _key ^= zobrist::key(c, p, s);

    if (p <= QUEEN)
        _pieceMaterial[c] -= Material[p];
    else
        _pawnKey ^= zobrist::key(c, p, s);
}

void Position::set(Color c, Piece p, Square s)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p, NB_PIECE);
    BOUNDS(s, NB_SQUARE);

    bb::set(_byColor[c], s);
    bb::set(_byPiece[p], s);

    _pieceOn[s] = p;
    _pst += pst::table[c][p][s];
    _key ^= zobrist::key(c, p, s);

    if (p <= QUEEN)
        _pieceMaterial[c] += Material[p];
    else
        _pawnKey ^= zobrist::key(c, p, s);
}

void Position::finish()
{
    const Color us = turn(), them = ~us;
    const Square ksq = king_square(*this, us);

    _attacked = attacked_by(them);
    _checkers = bb::test(_attacked, ksq) ? attackers_to(*this, ksq, pieces(*this)) & occ(them) : 0;
}

void Position::set(const std::string& fen)
{
    clear();
    std::istringstream is(fen);
    std::string token;

    // Piece placement
    is >> token;
    Square s = A8;

    for (char c : token) {
        if (isdigit(c))
            s += c - '0';
        else if (c == '/')
            s += 2 * DOWN;
        else {
            for (Color col = WHITE; col <= BLACK; ++col) {
                const Piece p = Piece(PieceLabel[col].find(c));

                if (unsigned(p) < NB_PIECE) {
                    set(col, p, s);
                    ++s;
                }
            }
        }
    }

    // Turn of play
    is >> token;

    if (token == "w")
        _turn = WHITE;
    else {
        _turn = BLACK;
        _key ^= zobrist::turn();
    }

    // Castling rights
    is >> token;

    if (token != "-") {
        for (char c : token) {
            const Rank r = isupper(c) ? RANK_1 : RANK_8;
            c = toupper(c);

            if (c == 'K')
                s = square(r, FILE_H);
            else if (c == 'Q')
                s = square(r, FILE_A);
            else if ('A' <= c && c <= 'H')
                s = square(r, File(c - 'A'));

            bb::set(_castlableRooks, s);
        }

        _key ^= zobrist::castling(castlable_rooks());
    }

    // En passant and 50 move
    is >> token;
    _epSquare = string_to_square(token);
    _key ^= zobrist::en_passant(ep_square());
    is >> _rule50;

    finish();
}

bitboard_t Position::occ(Color c) const
{
    BOUNDS(c, NB_COLOR);

    return _byColor[c];
}

bitboard_t Position::occ(Piece p) const
{
    BOUNDS(p, NB_PIECE);

    return _byPiece[p];
}

Color Position::turn() const
{
    return _turn;
}

Square Position::ep_square() const
{
    BOUNDS(_epSquare, NB_SQUARE+1);

    return _epSquare;
}

int Position::rule50() const
{
    assert(0 <= _rule50 && _rule50 <= 100);

    return _rule50;
}

bitboard_t Position::checkers() const
{
    assert(_checkers == (attackers_to(*this, king_square(*this, turn()),
                                      pieces(*this)) & occ(~turn())));

    return _checkers;
}

bitboard_t Position::attacked() const
{
    assert(_attacked == attacked_by(~turn()));

    return _attacked;
}

bitboard_t Position::castlable_rooks() const
{
    assert(castlable_rooks_ok());

    return _castlableRooks;
}

uint64_t Position::key() const
{
    assert(key_ok());

    return _key;
}

uint64_t Position::pawn_key() const
{
    assert(pawn_key_ok());

    return _pawnKey;
}

eval_t Position::pst() const
{
    assert(pst_ok());

    return _pst;
}

eval_t Position::piece_material() const
{
    assert(material_ok());

    return piece_material(WHITE) + piece_material(BLACK);
}

eval_t Position::piece_material(Color c) const
{
    BOUNDS(c, NB_COLOR);
    assert(material_ok());

    return _pieceMaterial[c];
}

Piece Position::piece_on(Square s) const
{
    BOUNDS(s, NB_SQUARE);

    return Piece(_pieceOn[s]);
}

void Position::set(const Position& before, Move m)
{
    *this = before;
    _rule50++;

    const Color us = turn(), them = ~us;
    const Piece p = piece_on(m.from);
    const Piece capture = piece_on(m.to);

    // Capture piece on to square (if any)
    if (capture != NB_PIECE) {
        _rule50 = 0;
        // Use color_on() instead of them, because we could be playing a KxR castling here
        clear(color_on(*this, m.to), capture, m.to);

        // Capturing a rook alters corresponding castling right
        if (capture == ROOK)
            _castlableRooks &= ~(1ULL << m.to);
    }

    // Move our piece
    clear(us, p, m.from);
    set(us, p, m.to);

    if (p == PAWN) {
        // reset rule50, and set epSquare
        const int push = push_inc(us);
        _rule50 = 0;
        _epSquare = m.to == m.from + 2 * push ? m.from + push : NB_SQUARE;

        // handle ep-capture and promotion
        if (m.to == before.ep_square())
            clear(them, p, m.to - push);
        else if (rank_of(m.to) == RANK_8 || rank_of(m.to) == RANK_1) {
            clear(us, p, m.to);
            set(us, m.prom, m.to);
        }
    } else {
        _epSquare = NB_SQUARE;

        if (p == ROOK)
            // remove corresponding castling right
            _castlableRooks &= ~(1ULL << m.from);
        else if (p == KING) {
            // Lose all castling rights
            _castlableRooks &= ~bb::rank(Rank(us * RANK_8));

            // Castling
            if (bb::test(before.occ(us), m.to)) {
                // Capturing our own piece can only be a castling move, encoded KxR
                assert(before.piece_on(m.to) == ROOK);
                const Rank r = rank_of(m.from);

                clear(us, KING, m.to);
                set(us, KING, square(r, m.to > m.from ? FILE_G : FILE_C));
                set(us, ROOK, square(r, m.to > m.from ? FILE_F : FILE_D));
            }
        }
    }

    _turn = them;
    _key ^= zobrist::turn();
    _key ^= zobrist::en_passant(before.ep_square()) ^ zobrist::en_passant(ep_square());
    _key ^= zobrist::castling(before.castlable_rooks() ^ castlable_rooks());

    finish();
}

void Position::toggle(const Position& before)
{
    *this = before;
    _epSquare = NB_SQUARE;

    _turn = ~turn();
    _key ^= zobrist::turn();
    _key ^= zobrist::en_passant(before.ep_square()) ^ zobrist::en_passant(ep_square());

    finish();
}

bitboard_t pieces(const Position& pos, Color c, Piece p)
{
    return pos.occ(c) & pos.occ(p);
}

bitboard_t pieces(const Position& pos, Piece p1, Piece p2)
{
    return pos.occ(p1) | pos.occ(p2);
}

bitboard_t pieces(const Position& pos)
{
    assert(!(pos.occ(WHITE) & pos.occ(BLACK)));

    return pos.occ(WHITE) | pos.occ(BLACK);
}

bitboard_t pieces(const Position& pos, Color c, Piece p1, Piece p2)
{
    return pos.occ(c) & (pos.occ(p1) | pos.occ(p2));
}

std::string get(const Position& pos)
{
    std::ostringstream os;

    // Piece placement
    for (Rank r = RANK_8; r >= RANK_1; --r) {
        int cnt = 0;

        for (File f = FILE_A; f <= FILE_H; ++f) {
            const Square s = square(r, f);

            if (bb::test(pieces(pos), s)) {
                if (cnt)
                    os << char(cnt + '0');

                os << PieceLabel[color_on(pos, s)][pos.piece_on(s)];
                cnt = 0;
            } else
                cnt++;
        }

        if (cnt)
            os << char(cnt + '0');

        os << (r == RANK_1 ? ' ' : '/');
    }

    // Turn of play
    os << (pos.turn() == WHITE ? "w " : "b ");

    // Castling rights
    if (!pos.castlable_rooks())
        os << '-';
    else {
        for (Color c = WHITE; c <= BLACK; ++c) {
            const bitboard_t sqs = pos.castlable_rooks() & pos.occ(c);

            if (!sqs)
                continue;

            const Square king = king_square(pos, c);

            // Because we have castlable rooks, the king has to be on the first rank and
            // cannot be in a corner, which allows using bb::ray(king, king +/- 1) to
            // search for the castle rook in Chess960.
            assert(rank_of(king) == relative_rank(c, RANK_1));
            assert(file_of(king) != FILE_A && file_of(king) != FILE_H);

            // Right side castling
            if (sqs & bb::ray(king, king + 1)) {
                if (Chess960)
                    os << char(file_of(bb::lsb(sqs & bb::ray(king, king + 1)))
                               + (c == WHITE ? 'A' : 'a'));
                else
                    os << PieceLabel[c][KING];
            }

            // Left side castling
            if (sqs & bb::ray(king, king - 1)) {
                if (Chess960)
                    os << char(file_of(bb::msb(sqs & bb::ray(king, king - 1)))
                               + (c == WHITE ? 'A' : 'a'));
                else
                    os << PieceLabel[c][QUEEN];
            }
        }
    }

    os << ' ';

    // En passant and 50 move
    os << (pos.ep_square() < NB_SQUARE ? square_to_string(pos.ep_square()) : "-") << ' ';
    os << pos.rule50();

    return os.str();
}

bitboard_t ep_square_bb(const Position& pos)
{
    return pos.ep_square() < NB_SQUARE ? 1ULL << pos.ep_square() : 0;
}

bool insufficient_material(const Position& pos)
{
    return bb::count(pieces(pos)) <= 3 && !pos.occ(PAWN) && !pos.occ(ROOK) && !pos.occ(QUEEN);
}

Square king_square(const Position& pos, Color c)
{
    assert(bb::count(pieces(pos, c, KING)) == 1);

    return bb::lsb(pieces(pos, c, KING));
}

Color color_on(const Position& pos, Square s)
{
    assert(bb::test(pieces(pos), s));

    return bb::test(pos.occ(WHITE), s) ? WHITE : BLACK;
}

bitboard_t attackers_to(const Position& pos, Square s, bitboard_t occ)
{
    BOUNDS(s, NB_SQUARE);

    return (pieces(pos, WHITE, PAWN) & bb::pattacks(BLACK, s))
           | (pieces(pos, BLACK, PAWN) & bb::pattacks(WHITE, s))
           | (bb::nattacks(s) & pos.occ(KNIGHT))
           | (bb::kattacks(s) & pos.occ(KING))
           | (bb::rattacks(s, occ) & pieces(pos, ROOK, QUEEN))
           | (bb::battacks(s, occ) & pieces(pos, BISHOP, QUEEN));
}

void print(const Position& pos)
{
    for (Rank r = RANK_8; r >= RANK_1; --r) {
        char line[] = ". . . . . . . .";

        for (File f = FILE_A; f <= FILE_H; ++f) {
            const Square s = square(r, f);
            line[2 * f] = bb::test(pieces(pos), s)
                          ? PieceLabel[color_on(pos, s)][pos.piece_on(s)]
                          : s == pos.ep_square() ? '*' : '.';
        }

        std::cout << line << '\n';
    }

    std::cout << get(pos) << std::endl;

    bitboard_t b = pos.checkers();

    if (b) {
        std::cout << "checkers:";

        while (b)
            std::cout << ' ' << square_to_string(bb::pop_lsb(b));

        std::cout << std::endl;
    }
}
