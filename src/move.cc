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
#include "move.h"
#include "bitboard.h"
#include "position.h"

bool move_ok(const Move& m)
{
    return unsigned(m.from) < NB_SQUARE && unsigned(m.to) < NB_SQUARE
           && ((KNIGHT <= m.prom && m.prom <= QUEEN) || m.prom == NB_PIECE);
}

Move::operator move_t() const
{
    assert(move_ok(*this));
    return from | (to << 6) | (prom << 12);
}

Move Move::operator =(move_t em)
{
    from = Square(em & 077);
    to = Square((em >> 6) & 077);
    prom = Piece(em >> 12);
    assert(move_ok(*this));
    return *this;
}

bool move_is_capture(const Position& pos, const Move& m)
{
    const Color us = pos.turn(), them = ~us;
    return (bb::test(pos.by_color(them), m.to))
           || ((m.to == pos.ep_square() || relative_rank_of(us, m.to) == RANK_8)
               && pos.piece_on(m.from) == PAWN);
}

bool move_is_castling(const Position& pos, const Move& m)
{
    return bb::test(pos.by_color(pos.turn()), m.to);
}

std::string move_to_string(const Position& pos, const Move& m)
{
    assert(move_ok(m));

    std::string s;

    if (!(m.from | m.to | m.prom))
        return "0000";

    const Square _tsq = !Chess960 && move_is_castling(pos, m)
                        ? (m.to > m.from ? m.from + 2 : m.from - 2)    // e1h1 -> e1g1, e1a1 -> e1c1
                        : m.to;

    s += file_of(m.from) + 'a';
    s += rank_of(m.from) + '1';
    s += file_of(_tsq) + 'a';
    s += rank_of(_tsq) + '1';

    if (m.prom < NB_PIECE)
        s += PieceLabel[BLACK][m.prom];

    return s;
}

void move_from_string(const Position& pos, const std::string& s, Move& m)
{
    m.from = square(Rank(s[1] - '1'), File(s[0] - 'a'));
    m.to = square(Rank(s[3] - '1'), File(s[2] - 'a'));
    m.prom = s[4] ? (Piece)PieceLabel[BLACK].find(s[4]) : NB_PIECE;

    if (!Chess960 && pos.piece_on(m.from) == KING) {
        if (m.to == m.from + 2)  // e1g1 -> e1h1
            ++m.to;
        else if (m.to == m.from - 2)  // e1c1 -> e1a1
            m.to -= 2;
    }

    assert(move_ok(m));
}

bool move_is_legal(const Position& pos, const Move& m)
{
    const Piece p = pos.piece_on(m.from);
    const Square king = king_square(pos, pos.turn());

    if (p == KING) {
        if (bb::test(pos.by_color(pos.turn()), m.to)) {
            // Castling: king must not move through attacked square, and rook must not
            // be pinned
            assert(pos.piece_on(m.to) == ROOK);
            const Square _tsq = square(rank_of(m.from), m.from < m.to ? FILE_G : FILE_C);
            return !(pos.attacked() & bb::segment(m.from, _tsq))
                   && !bb::test(pos.pins(), m.to);
        } else
            // Normal king move: do not land on an attacked square
            return !bb::test(pos.attacked(), m.to);
    } else {
        // Normal case: illegal if pinned, and moves out of pin-ray
        if (bb::test(pos.pins(), m.from) && !bb::test(bb::ray(king, m.from), m.to))
            return false;

        // En-passant special case: also illegal if self-check through the en-passant
        // captured pawn
        if (m.to == pos.ep_square() && p == PAWN) {
            const Color us = pos.turn(), them = ~us;
            bitboard_t occ = pieces(pos);
            bb::clear(occ, m.from);
            bb::set(occ, m.to);
            bb::clear(occ, m.to + push_inc(them));
            return !(bb::rattacks(king, occ) & pieces_cpp(pos, them, ROOK, QUEEN))
                   && !(bb::battacks(king, occ) & pieces_cpp(pos, them, BISHOP, QUEEN));
        } else
            return true;
    }
}

int move_see(const Position& pos, const Move& m)
{
    const int see_value[NB_PIECE+1] = {N, B, R, Q, MATE, P, 0};

    Color us = pos.turn();
    bitboard_t occ = pieces(pos);

    // General case
    int gain[32] = {see_value[pos.piece_on(m.to)]};
    Piece capture = pos.piece_on(m.from);
    bb::clear(occ, m.from);

    // Special cases
    if (capture == PAWN) {
        if (m.to == pos.ep_square()) {
            bb::clear(occ, m.to - push_inc(us));
            gain[0] = see_value[capture];
        } else if (relative_rank_of(us, m.to) == RANK_8)
            gain[0] += see_value[capture = m.prom] - see_value[PAWN];
    }

    // Easy case: to is not defended
    // TODO: explore performance tradeoff between using pos.attacked() and using attackers below
    if (!bb::test(pos.attacked(), m.to))
        return gain[0];

    bitboard_t attackers = attackers_to(pos, m.to, occ);
    bitboard_t our_attackers;

    int idx = 0;

    while (us = ~us, our_attackers = attackers & pos.by_color(us)) {
        // Find least valuable attacker (LVA)
        Piece p = PAWN;

        if (!(our_attackers & pieces_cp(pos, us, PAWN))) {
            for (p = KNIGHT; p <= KING; ++p) {
                if (our_attackers & pieces_cp(pos, us, p))
                    break;
            }
        }

        // Remove the LVA
        bb::clear(occ, bb::lsb(our_attackers & pieces_cp(pos, us, p)));

        // Scan for new X-ray attacks through the LVA
        if (p != KNIGHT) {
            attackers |= (pos.by_piece(BISHOP) | pos.by_piece(QUEEN))
                         & bb::bpattacks(m.to) & bb::battacks(m.to, occ);
            attackers |= (pos.by_piece(ROOK) | pos.by_piece(QUEEN))
                         & bb::rpattacks(m.to) & bb::rattacks(m.to, occ);
        }

        // Remove attackers we've already done
        attackers &= occ;

        // Add the new entry to the gain[] array
        idx++;
        assert(idx < 32);
        gain[idx] = see_value[capture] - gain[idx-1];

        if (p == PAWN && relative_rank_of(us, m.to) == RANK_8) {
            gain[idx] += see_value[QUEEN] - see_value[PAWN];
            capture = QUEEN;
        } else
            capture = p;
    }

    do {
        if (-gain[idx] < gain[idx-1])
            gain[idx-1] = -gain[idx];
    } while (--idx);

    return gain[0];
}
