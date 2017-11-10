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

bool move_ok(move_t m)
{
    const int from = move_from(m), to = move_to(m), prom = move_prom(m);
    return bb_test(RPseudoAttacks[from] | BPseudoAttacks[from] | NAttacks[from], to) && from != to
        && ((unsigned)prom <= QUEEN || prom == NB_PIECE);
}

int move_from(move_t m)
{
    return m & 077;
}

int move_to(move_t m)
{
    return (m >> 6) & 077;
}

int move_from_to(move_t m)
{
    return m & 07777;
}

int move_prom(move_t m)
{
    return m >> 12;
}

move_t move_build(int from, int to, int prom)
{
    move_t m = from | (to << 6) | (prom << 12);
    assert(move_ok(m));
    return m;
}

bool move_is_capture(const Position *pos, move_t m)
{
    assert(move_ok(m));
    const int us = pos->turn, them = opposite(us);
    const int from = move_from(m), to = move_to(m);
    return bb_test(pos->byColor[them], to)
        || ((to == pos->epSquare || relative_rank_of(us, to) == RANK_8)
            && pos->pieceOn[from] == PAWN);
}

bool move_is_castling(const Position *pos, move_t m)
{
    return bb_test(pos->byColor[pos->turn], move_to(m));
}

void move_to_string(const Position *pos, move_t m, char *str)
{
    assert(move_ok(m));
    const int from = move_from(m), to = move_to(m), prom = move_prom(m);

    if (!(from | to | prom)) {
        strcpy(str, "0000");
        return;
    }

    const int _to = !pos->chess960 && move_is_castling(pos, m)
        ? (to > from ? from + 2 : from - 2)  // e1h1 -> e1g1, e1a1 -> e1c1
        : to;

    *str++ = file_of(from) + 'a';
    *str++ = rank_of(from) + '1';
    *str++ = file_of(_to) + 'a';
    *str++ = rank_of(_to) + '1';

    if (prom < NB_PIECE)
        *str++ = PieceLabel[BLACK][prom];

    *str = '\0';
}

move_t string_to_move(const Position *pos, const char *str)
{
    const int prom = str[4] ? (int)(strchr(PieceLabel[BLACK], str[4]) - PieceLabel[BLACK]) : NB_PIECE;
    const int from = square(str[1] - '1', str[0] - 'a');
    int to = square(str[3] - '1', str[2] - 'a');

    if (!pos->chess960 && pos->pieceOn[from] == KING) {
        if (to == from + 2)  // e1g1 -> e1h1
            ++to;
        else if (to == from - 2)  // e1c1 -> e1a1
            to -= 2;
    }

    return move_build(from, to, prom);
}

bool move_is_legal(const Position *pos, move_t m)
{
    const int from = move_from(m), to = move_to(m);
    const int p = pos->pieceOn[from];
    const int king = pos_king_square(pos, pos->turn);

    if (p == KING) {
        if (bb_test(pos->byColor[pos->turn], to)) {
            // Castling: king can't move through attacked square, and rook can't be pinned
            assert(pos->pieceOn[to] == ROOK);
            const int _tsq = square(rank_of(from), from < to ? FILE_G : FILE_C);
            return !(pos->attacked & Segment[from][_tsq])
                && !bb_test(pos->pins, to);
        } else
            // Normal king move: do not land on an attacked square
            return !bb_test(pos->attacked, to);
    } else {
        // Normal case: illegal if pinned, and moves out of pin-ray
        if (bb_test(pos->pins, from) && !bb_test(Ray[king][from], to))
            return false;

        // En-passant special case: also illegal if self-check through the en-passant captured pawn
        if (to == pos->epSquare && p == PAWN) {
            const int us = pos->turn, them = opposite(us);
            bitboard_t occ = pos_pieces(pos);
            bb_clear(&occ, from);
            bb_set(&occ, to);
            bb_clear(&occ, to + push_inc(them));
            return !(bb_rattacks(king, occ) & pos_pieces_cpp(pos, them, ROOK, QUEEN))
                && !(bb_battacks(king, occ) & pos_pieces_cpp(pos, them, BISHOP, QUEEN));
        } else
            return true;
    }
}

int move_see(const Position *pos, move_t m)
{
    const int see_value[] = {N, B, R, Q, MATE, P, 0};

    assert(move_ok(m));
    const int from = move_from(m), to = move_to(m), prom = move_prom(m);
    int us = pos->turn;
    bitboard_t occ = pos_pieces(pos);

    // General case
    int gain[32] = {see_value[(int)pos->pieceOn[to]]};
    int capture = pos->pieceOn[from];
    bb_clear(&occ, from);

    // Special cases
    if (capture == PAWN) {
        if (to == pos->epSquare) {
            bb_clear(&occ, to - push_inc(us));
            gain[0] = see_value[capture];
        } else if (relative_rank_of(us, to) == RANK_8)
            gain[0] += see_value[capture = prom] - see_value[PAWN];
    }

    // Easy case: to is not defended
    if (!bb_test(pos->attacked, to))
        return gain[0];

    bitboard_t attackers = pos_attackers_to(pos, to, occ);
    bitboard_t ourAttackers;

    int idx = 0;

    while (us = opposite(us), ourAttackers = attackers & pos->byColor[us]) {
        // Find least valuable attacker (LVA)
        int p = PAWN;

        if (!(ourAttackers & pos_pieces_cp(pos, us, PAWN))) {
            for (p = KNIGHT; p <= KING; p++) {
                if (ourAttackers & pos_pieces_cp(pos, us, p))
                    break;
            }
        }

        // Remove the LVA
        bb_clear(&occ, bb_lsb(ourAttackers & pos_pieces_cp(pos, us, p)));

        // Remove attackers we've already done
        attackers &= occ;

        // Add the new entry to the gain[] array
        idx++;
        assert(idx < 32);
        gain[idx] = see_value[capture] - gain[idx - 1];

        if (p == PAWN && relative_rank_of(us, to) == RANK_8) {
            gain[idx] += see_value[QUEEN] - see_value[PAWN];
            capture = QUEEN;
        } else
            capture = p;
    }

    do {
        if (-gain[idx] < gain[idx - 1])
            gain[idx - 1] = -gain[idx];
    } while (--idx);

    return gain[0];
}
