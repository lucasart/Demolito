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

bitboard_t PinInfo::hidden_checkers(const Position& pos, Color attacker, Color blocker) const
{
    const int ksq = pos.king_square(~attacker);
    bitboard_t pinners = (pos.occ_RQ(attacker) & bb::rpattacks(ksq))
                         | (pos.occ_BQ(attacker) & bb::bpattacks(ksq));

    bitboard_t result = 0;

    while (pinners) {
        const int sq = bb::pop_lsb(pinners);
        bitboard_t skewered = bb::segment(ksq, sq) & pos.occ();
        bb::clear(skewered, ksq);
        bb::clear(skewered, sq);

        if (!bb::several(skewered) && (skewered & pos.occ(blocker)))
            result |= skewered;
    }

    return result;
}

PinInfo::PinInfo(const Position& pos)
{
    const Color us = pos.turn(), them = ~us;
    pinned = hidden_checkers(pos, them, us);
    discoCheckers = hidden_checkers(pos, us, us);
}

bool Move::ok() const
{
    return unsigned(fsq) < NB_SQUARE && unsigned(tsq) < NB_SQUARE
           && ((KNIGHT <= prom && prom <= QUEEN) || prom == NB_PIECE);
}

Move::operator move_t() const
{
    assert(ok());
    return fsq | (tsq << 6) | (prom << 12);
}

Move Move::operator =(move_t em)
{
    fsq = em & 077;
    tsq = (em >> 6) & 077;
    prom = Piece(em >> 12);
    assert(ok());
    return *this;
}

bool Move::is_capture(const Position& pos) const
{
    const Color us = pos.turn(), them = ~us;
    return (bb::test(pos.occ(them), tsq))
           || ((tsq == pos.ep_square() || relative_rank(us, tsq) == RANK_8)
               && pos.piece_on(fsq) == PAWN);
}

bool Move::is_castling(const Position& pos) const
{
    return bb::test(pos.occ(pos.turn()), tsq);
}

std::string Move::to_string(const Position& pos) const
{
    assert(ok());

    std::string s;

    if (null())
        return "0000";

    const int _tsq = !Chess960 && is_castling(pos)
                     ? (tsq > fsq ? fsq + 2 : fsq - 2)    // e1h1 -> e1g1, e1c1 -> e1a1
                     : tsq;

    s += file_of(fsq) + 'a';
    s += rank_of(fsq) + '1';
    s += file_of(_tsq) + 'a';
    s += rank_of(_tsq) + '1';

    if (prom < NB_PIECE)
        s += PieceLabel[BLACK][prom];

    return s;
}

void Move::from_string(const Position& pos, const std::string& s)
{
    fsq = square(Rank(s[1] - '1'), File(s[0] - 'a'));
    tsq = square(Rank(s[3] - '1'), File(s[2] - 'a'));
    prom = s[4] ? (Piece)PieceLabel[BLACK].find(s[4]) : NB_PIECE;

    if (!Chess960 && pos.piece_on(fsq) == KING) {
        if (tsq == fsq + 2)      // e1g1
            tsq++;               // -> e1h1
        else if (tsq == fsq - 2) // e1c1
            tsq -= 2;            // -> e1a1
    }

    assert(ok());
}

bool Move::pseudo_is_legal(const Position& pos, const PinInfo& pi) const
{
    const Piece p = pos.piece_on(fsq);
    const int ksq = pos.king_square(pos.turn());

    if (p == KING) {
        if (bb::test(pos.occ(pos.turn()), tsq)) {
            // Castling: king must not move through attacked square, and rook must not
            // be pinned
            assert(pos.piece_on(tsq) == ROOK);
            const int _tsq = square(rank_of(fsq), fsq < tsq ? FILE_G : FILE_C);
            return !(pos.attacked() & bb::segment(fsq, _tsq))
                   && !bb::test(pi.pinned, tsq);
        } else
            // Normal king move: do not land on an attacked square
            return !bb::test(pos.attacked(), tsq);
    } else {
        // Normal case: illegal if pinned, and moves out of pin-ray
        if (bb::test(pi.pinned, fsq) && !bb::test(bb::ray(ksq, fsq), tsq))
            return false;

        // En-passant special case: also illegal if self-check through the en-passant
        // captured pawn
        if (tsq == pos.ep_square() && p == PAWN) {
            const Color us = pos.turn(), them = ~us;
            bitboard_t occ = pos.occ();
            bb::clear(occ, fsq);
            bb::set(occ, tsq);
            bb::clear(occ, tsq + push_inc(them));
            return !(bb::rattacks(ksq, occ) & pos.occ_RQ(them))
                   && !(bb::battacks(ksq, occ) & pos.occ_BQ(them));
        } else
            return true;
    }
}

int Move::see(const Position& pos) const
{
    const int see_value[] = {N, B, R, Q, MATE, P, 0};

    Color us = pos.turn();
    bitboard_t occ = pos.occ();

    // General case
    int gain[32] = {see_value[pos.piece_on(tsq)]};
    Piece capture = pos.piece_on(fsq);
    bb::clear(occ, fsq);

    // Special cases
    if (capture == PAWN) {
        if (tsq == pos.ep_square()) {
            bb::clear(occ, tsq - push_inc(us));
            gain[0] = see_value[capture];
        } else if (relative_rank(us, tsq) == RANK_8)
            gain[0] += see_value[capture = prom] - see_value[PAWN];
    }

    // Easy case: tsq is not defended
    // TODO: explore performance tradeoff between using pos.attacked() and using attackers below
    if (!bb::test(pos.attacked(), tsq))
        return gain[0];

    bitboard_t attackers = pos.attackers_to(tsq, occ);
    bitboard_t our_attackers;

    int idx = 0;

    while (us = ~us, our_attackers = attackers & pos.occ(us)) {
        // Find least valuable attacker (LVA)
        Piece p = PAWN;

        if (!(our_attackers & pos.occ(us, PAWN))) {
            for (p = KNIGHT; p <= KING; ++p) {
                if (our_attackers & pos.occ(us, p))
                    break;
            }
        }

        // Remove the LVA
        bb::clear(occ, bb::lsb(our_attackers & pos.occ(us, p)));

        // Scan for new X-ray attacks through the LVA
        if (p != KNIGHT) {
            attackers |= (pos.occ(BISHOP) | pos.occ(QUEEN))
                         & bb::bpattacks(tsq) & bb::battacks(tsq, occ);
            attackers |= (pos.occ(ROOK) | pos.occ(QUEEN))
                         & bb::rpattacks(tsq) & bb::rattacks(tsq, occ);
        }

        // Remove attackers we've already done
        attackers &= occ;

        // Add the new entry to the gain[] array
        idx++;
        assert(idx < 32);
        gain[idx] = see_value[capture] - gain[idx-1];

        if (p == PAWN && relative_rank(us, tsq) == RANK_8) {
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
