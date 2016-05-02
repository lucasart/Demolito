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
#include "position.h"
#include "bitboard.h"
#include "pst.h"
#include "gen.h"

bool Position::key_ok() const
{
    uint64_t k = 0;

    for (int color = 0; color < NB_COLOR; color++)
    for (int piece = 0; piece < NB_PIECE; piece++) {
        bitboard_t b = occ(color, piece);
        while (b)
            k ^= zobrist::key(color, piece, bb::pop_lsb(b));
    }

    k ^= zobrist::en_passant(ep_square());
    k ^= zobrist::castling(castlable_rooks());

    if (turn() == BLACK)
        k ^= zobrist::turn();

    return k == _key;
}

bool Position::pst_ok() const
{
    eval_t sum = {0, 0};

    for (int color = 0; color < NB_COLOR; color++)
    for (int piece = 0; piece < NB_PIECE; piece++) {
        bitboard_t b = occ(color, piece);
        while (b)
            sum += pst::table[color][piece][bb::pop_lsb(b)];
    }

    return _pst == sum;
}

bool Position::material_ok() const
{
    eval_t npm[NB_COLOR] = {{0,0}, {0,0}};

    for (int color = 0; color < NB_COLOR; color++) {
        for (int piece = KNIGHT; piece <= QUEEN; piece++)
            npm[color] += Material[piece] * bb::count(occ(color, piece));
        if (npm[color] != _pieceMaterial[color])
            return false;
    }

    return true;
}

bitboard_t Position::attackers_to(int sq, bitboard_t _occ) const
{
    return (occ(WHITE, PAWN) & bb::pattacks(BLACK, sq))
        | (occ(BLACK, PAWN) & bb::pattacks(WHITE, sq))
        | (bb::nattacks(sq) & by_piece(KNIGHT))
        | (bb::kattacks(sq) & by_piece(KING))
        | (bb::rattacks(sq, _occ) & (by_piece(ROOK) | by_piece(QUEEN)))
        | (bb::battacks(sq, _occ) & (by_piece(BISHOP) | by_piece(QUEEN)));
}

bitboard_t Position::attacked_by(int color) const
{
    assert(color_ok(color));
    bitboard_t fss, result;

    // King and Knight attacks
    result = bb::kattacks(king_square(color));
    fss = occ(color, KNIGHT);
    while (fss)
        result |= bb::nattacks(bb::pop_lsb(fss));

    // Pawn captures
    fss = occ(color, PAWN) & ~bb::file(FILE_A);
    result |= bb::shift(fss, push_inc(color) + LEFT);
    fss = occ(color, PAWN) & ~bb::file(FILE_H);
    result |= bb::shift(fss, push_inc(color) + RIGHT);

    // Sliders
    bitboard_t _occ = occ() ^ occ(opp_color(color), KING);
    fss = occ_RQ(color);
    while (fss)
        result |= bb::rattacks(bb::pop_lsb(fss), _occ);
    fss = occ_BQ(color);
    while (fss)
        result |= bb::battacks(bb::pop_lsb(fss), _occ);

    return result;
}

void Position::clear()
{
    std::memset(this, 0, sizeof(*this));
    for (int sq = 0; sq < NB_SQUARE; sq++)
        _pieceOn[sq] = NB_PIECE;
}

void Position::clear(int color, int piece, int sq)
{
    assert(color_ok(color) && piece_ok(piece) && square_ok(sq));
    bb::clear(_byColor[color], sq);
    bb::clear(_byPiece[piece], sq);

    _pieceOn[sq] = NB_PIECE;
    _pst -= pst::table[color][piece][sq];
    if (piece <= QUEEN)
        _pieceMaterial[color] -= Material[piece];

    _key ^= zobrist::key(color, piece, sq);
}

void Position::set(int color, int piece, int sq)
{
    assert(color_ok(color) && piece_ok(piece) && square_ok(sq));
    bb::set(_byColor[color], sq);
    bb::set(_byPiece[piece], sq);

    _pieceOn[sq] = piece;
    _pst += pst::table[color][piece][sq];
    if (piece <= QUEEN)
        _pieceMaterial[color] += Material[piece];

    _key ^= zobrist::key(color, piece, sq);
}

void Position::finish()
{
    if (turn() == BLACK)
        _key ^= zobrist::turn();

    _key ^= zobrist::en_passant(ep_square());
    _key ^= zobrist::castling(castlable_rooks());

    _attacked = attacked_by(opp_color(turn()));
    _checkers = attackers_to(king_square(turn()), occ()) & occ(opp_color(turn()));

    assert(key_ok());
}

void Position::set(const std::string& fen)
{
    clear();
    std::istringstream is(fen);
    std::string s;

    // Piece placement
    is >> s;
    int sq = A8;
    for (char c : s) {
        if (isdigit(c))
            sq += c - '0';
        else if (c == '/')
            sq += 2 * DOWN;
        else {
            for (int color = 0; color < NB_COLOR; color++) {
                const int piece = PieceLabel[color].find(c);
                if (piece_ok(piece))
                    set(color, piece, sq++);
            }
        }
    }

    // Turn of play
    is >> s;
    _turn = s == "w" ? WHITE : BLACK;

    // Castling rights
    is >> s;
    if (s != "-") {
        for (char c : s) {
            int color = isupper(c) ? WHITE : BLACK;
            int r = RANK_8 * color;
            c = toupper(c);

            if (c == 'K')
                sq = square(r, FILE_H);
            else if (c == 'Q')
                sq = square(r, FILE_A);
            else if ('A' <= c && c <= 'H')
                sq = square(r, c - 'A');

            bb::set(_castlableRooks, sq);
        }
    }

    // En passant and 50 move
    is >> s;
    _epSquare = string_to_square(s);
    is >> _rule50;

    finish();
}

std::string Position::get() const
{
    std::ostringstream os;

    // Piece placement
    for (int r = RANK_8; r >= RANK_1; r--) {
        int cnt = 0;

        for (int f = FILE_A; f <= FILE_H; f++) {
            const int sq = square(r, f);

            if (bb::test(occ(), sq)) {
                const int color = color_on(sq), piece = piece_on(sq);
                if (cnt)
                    os << char(cnt + '0');
                cnt = 0;
                os << PieceLabel[color][piece];
            } else
                cnt++;
        }

        if (cnt)
            os << char(cnt + '0');
        os << (r == RANK_1 ? ' ' : '/');
    }

    // Turn of play
    os << (turn() == WHITE ? "w " : "b ");

    // Castling rights
    if (!castlable_rooks())
        os << '-';
    else {
        for (int color = WHITE; color <= BLACK; color++) {
            const bitboard_t sqs = castlable_rooks() & occ(color);
            if (!sqs)
                continue;

            // Because we have castlable rooks, king has to be on the first rank and not
            // in a corner, which allows using bb::ray(ksq, ksq +/- 1) to search for the
            // castle rook in Chess960.
            const int ksq = king_square(color);
            assert(rank_of(ksq) == (color == WHITE ? RANK_1 : RANK_8));
            assert(file_of(ksq) != FILE_A && file_of(ksq) != FILE_H);

            // Right side castling
            if (sqs & bb::ray(ksq, ksq + 1)) {
                if (Chess960)
                    os << char(file_of(bb::lsb(sqs & bb::ray(ksq, ksq + 1)))
                        + (color == WHITE ? 'A' : 'a'));
                else
                    os << PieceLabel[color][KING];
            }

            // Left side castling
            if (sqs & bb::ray(ksq, ksq - 1)) {
                if (Chess960)
                    os << char(file_of(bb::msb(sqs & bb::ray(ksq, ksq - 1)))
                        + (color == WHITE ? 'A' : 'a'));
                else
                    os << PieceLabel[color][QUEEN];
            }
        }
    }
    os << ' ';

    // En passant and 50 move
    os << (square_ok(ep_square()) ? square_to_string(ep_square()) : "-") << ' ';
    os << rule50();

    return os.str();
}

bitboard_t Position::occ() const
{
    assert(!(_byColor[WHITE] & _byColor[BLACK]));
    assert((_byColor[WHITE] | _byColor[BLACK]) == (by_piece(KNIGHT) | by_piece(BISHOP)
        | by_piece(ROOK) | by_piece(QUEEN) | by_piece(KING) | by_piece(PAWN)));
    return _byColor[WHITE] | _byColor[BLACK];
}

bitboard_t Position::occ(int color) const
{
    assert(color_ok(color));
    return _byColor[color];
}

bitboard_t Position::occ(int color, int piece) const
{
    assert(color_ok(color) && piece_ok(piece));
    return occ(color) & by_piece(piece);
}

bitboard_t Position::by_piece(int piece) const
{
    assert(piece_ok(piece));
    return _byPiece[piece];
}

bitboard_t Position::occ_RQ(int color) const
{
    assert(color_ok(color));
    return occ(color) & (by_piece(ROOK) | by_piece(QUEEN));
}

bitboard_t Position::occ_BQ(int color) const
{
    assert(color_ok(color));
    return occ(color) & (by_piece(BISHOP) | by_piece(QUEEN));
}

int Position::turn() const
{
    assert(color_ok(_turn));
    return _turn;
}

int Position::ep_square() const
{
    assert(square_ok(_epSquare) || _epSquare == NB_SQUARE);
    return _epSquare;
}

bitboard_t Position::ep_square_bb() const
{
    // Guard against oversized shift
    return square_ok(ep_square()) ? 1ULL << ep_square() : 0;
}

int Position::rule50() const
{
    // NB: rule50() = 100 is ok, if (and only if) position is check mate
    assert(0 <= _rule50 && _rule50 <= 100);
    return _rule50;
}

bitboard_t Position::checkers() const
{
    assert(_checkers == (attackers_to(king_square(turn()), occ()) & occ(opp_color(turn()))));
    return _checkers;
}

bitboard_t Position::attacked() const
{
    assert(_attacked == attacked_by(opp_color(turn())));
    return _attacked;
}

bitboard_t Position::castlable_rooks() const
{
    // TODO: verify _castlableRooks
    return _castlableRooks;
}

uint64_t Position::key() const
{
    assert(key_ok());
    return _key;
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

eval_t Position::piece_material(int color) const
{
    assert(material_ok());
    return _pieceMaterial[color];
}

int Position::king_square(int color) const
{
    return bb::lsb(occ(color, KING));
}

int Position::color_on(int sq) const
{
    assert(bb::test(occ(), sq));
    return bb::test(occ(WHITE), sq) ? WHITE : BLACK;
}

int Position::piece_on(int sq) const
{
    return _pieceOn[sq];
}

void Position::set(const Position& before, Move m)
{
    *this = before;
    _rule50++;

    const int us = turn(), them = opp_color(us);
    const int piece = piece_on(m.fsq);
    const int capture = piece_on(m.tsq);

    // Capture piece on to square (if any)
    if (capture != NB_PIECE) {
        _rule50 = 0;
        // Use color_on() instead of them, because we could be playing a KxR castling here
        clear(color_on(m.tsq), capture, m.tsq);

        // Capturing a rook alters corresponding castling right
        if (capture == ROOK)
            _castlableRooks &= ~(1ULL << m.tsq);
    }

    // Move our piece
    clear(us, piece, m.fsq);
    set(us, piece, m.tsq);

    if (piece == PAWN) {
        // reset rule50, and set epSquare
        const int push = push_inc(us);
        _rule50 = 0;
        _epSquare = m.tsq == m.fsq + 2 * push ? m.fsq + push : NB_SQUARE;

        // handle ep-capture and promotion
        if (m.tsq == before.ep_square())
            clear(them, piece, m.tsq - push);
        else if (rank_of(m.tsq) == RANK_8 || rank_of(m.tsq) == RANK_1) {
            clear(us, piece, m.tsq);
            set(us, m.prom, m.tsq);
        }
    } else {
        _epSquare = NB_SQUARE;

        if (piece == ROOK)
            // remove corresponding castling right
            _castlableRooks &= ~(1ULL << m.fsq);
        else if (piece == KING) {
            // Lose all castling rights
            _castlableRooks &= ~bb::rank(us * RANK_8);

            // Castling
            if (bb::test(before.occ(us), m.tsq)) {
                // Capturing our own piece can only be a castling move, encoded KxR
                assert(before.piece_on(m.tsq) == ROOK);
                const int r = rank_of(m.fsq);

                clear(us, KING, m.tsq);
                set(us, KING, m.tsq > m.fsq ? square(r, FILE_G) : square(r, FILE_C));
                set(us, ROOK, m.tsq > m.fsq ? square(r, FILE_F) : square(r, FILE_D));
            }
        }
    }

    _turn = them;
    _key ^= zobrist::turn();
    _key ^= zobrist::en_passant(before.ep_square()) ^ zobrist::en_passant(ep_square());
    _key ^= zobrist::castling(before.castlable_rooks() ^ castlable_rooks());

    _attacked = attacked_by(opp_color(turn()));
    _checkers = attackers_to(king_square(turn()), occ()) & occ(opp_color(turn()));

    assert(key_ok());
}

void Position::print() const
{
    for (int r = RANK_8; r >= RANK_1; r--) {
        char line[] = ". . . . . . . .";
        for (int f = FILE_A; f <= FILE_H; f++) {
            const int sq = square(r, f);
            line[2 * f] = bb::test(occ(), sq)
                ? PieceLabel[color_on(sq)][piece_on(sq)]
                : sq == ep_square() ? '*' : '.';
        }
        std::cout << line << '\n';
    }
    std::cout << get() << std::endl;

    bitboard_t b = checkers();
    if (b) {
        std::cout << "checkers:";
        while (b)
            std::cout << ' ' << square_to_string(bb::pop_lsb(b));
        std::cout << std::endl;
    }
}

void Position::random(zobrist::PRNG& prng, int pieces)
{
    // Piece frequency
    #define NB_PIECE_FREQ 8 + 2 * 3 + 1
    const int PieceFrequency[NB_PIECE_FREQ] = {
        PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN,
        KNIGHT, KNIGHT, BISHOP, BISHOP, ROOK, ROOK,
        QUEEN
    };

    // Rank frequency
    #define NB_RANK_FREQ 8 * (8 + 1) / 2
    const int RankFrequency[NB_RANK_FREQ] = {
        RANK_1, RANK_1, RANK_1, RANK_1, RANK_1, RANK_1, RANK_1, RANK_1,
        RANK_2, RANK_2, RANK_2, RANK_2, RANK_2, RANK_2, RANK_2,
        RANK_3, RANK_3, RANK_3, RANK_3, RANK_3, RANK_3,
        RANK_4, RANK_4, RANK_4, RANK_4, RANK_4,
        RANK_5, RANK_5, RANK_5, RANK_5,
        RANK_6, RANK_6, RANK_6,
        RANK_7, RANK_7,
        RANK_8
    };

again:
    clear();

    // White King on a random square
    set(WHITE, KING, prng.rand() % NB_SQUARE);

    int sq;

    // Black King on an empty random square
    while (bb::test(occ(), sq = prng.rand() % NB_SQUARE));
        set(BLACK, KING, sq);

    // Pieces
    for (int i = 0; i < pieces; i++) {
        // Random color and piece. Skew the dice to the same relative piece frequency
        // as in the starting position.
        const int color = prng.rand() % NB_COLOR;
        const int piece = PieceFrequency[prng.rand() % NB_PIECE_FREQ];

        // Choose a random empty square. Pawns: skew the dice to get 3x more 2nd rank.
        do {
            const int f = prng.rand() % NB_FILE;
            int r = RankFrequency[prng.rand() % NB_RANK_FREQ];

            if (piece == PAWN && (r == RANK_1 || r == RANK_8))
                r = color ? RANK_7 : RANK_2;

            sq = square(color ? RANK_8 - r : r, f);
        } while (bb::test(occ(), sq));

        set(color, piece, sq);
    }

    // Determine turn, considering checks
    _turn = prng.rand() % NB_COLOR;
    bool checked[NB_COLOR];

    for (int color = 0; color < NB_COLOR; color++) {
        checked[color] = bb::test(attacked_by(opp_color(color)), king_square(color));

        // The side in check, if any, must have the move
        if (checked[color])
            _turn = color;
    }

    // Both sides can't be in check at the same time
    if (checked[WHITE] && checked[BLACK])
        goto again;

    // En passant
    _epSquare = NB_SQUARE;

    // Find possible en-passant squares
    int them = opp_color(turn());
    bitboard_t b = bb::rank(them ? RANK_5 : RANK_4) & occ(them, PAWN);
    b = bb::shift(b, them ? 8 : -8) & ~occ();
    b &= ~bb::shift(occ(), them ? -8 : 8);

    // Choose an en-passant square
    if (b) {
        // There are possible en-passant squares to choose from. With 50% probability,
        // we choose a one of them at random, and with 50% probability we choose no
        // en-passant square.
        do {
            b &= prng.rand();
        } while (bb::several(b));

        if (b) {
            assert(bb::count(b) == 1);
            _epSquare = bb::lsb(b);
        }
    }

    // Castling
    for (int color = 0; color < NB_COLOR; color++) {
        const int ksq = king_square(color);
        const int r = color ? RANK_8 : RANK_1;

        if (rank_of(ksq) != r && (Chess960 || file_of(ksq) != FILE_E))
            continue;

        const int edgeFile[2] = {FILE_A, FILE_H};
        const int direction[2] = {-1, +1};

        for (int i = 0; i < 2; i++) {
            if (file_of(ksq) != edgeFile[i]) {
                if (Chess960)
                    b = bb::ray(ksq, ksq + direction[i]) & occ(color, ROOK);
                else
                    b = (1ULL << square(r, edgeFile[i])) & occ(color, ROOK);

                do {
                    b &= prng.rand();
                } while (bb::several(b));

                _castlableRooks |= b;
            }
        }
    }

    _rule50 = prng.rand() % 100;
    finish();

    // Filter out positions with no legal move (ie. mate or stalemate)
    move_t emList[MAX_MOVES], *m, *end;

    end = gen::all_moves(*this, emList);

    if (emList == end)
        // no pseudo-legal moves
        goto again;

    const PinInfo pi(*this);

    for (m = emList; m != end; m++)
        if (Move(*m).pseudo_is_legal(*this, pi))
            break;

    if (m == end)
        // all pseudo-legal moves are illegal (mate or stalemate)
        goto again;
}
