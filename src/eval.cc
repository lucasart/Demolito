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
#include "eval.h"

namespace {

bitboard_t pawn_attacks(const Position& pos, Color c)
{
    const bitboard_t pawns = pos.occ(c, PAWN);
    return bb::shift(pawns & ~bb::file(FILE_A), push_inc(c) + LEFT)
           | bb::shift(pawns & ~bb::file(FILE_H), push_inc(c) + RIGHT);
}

eval_t score_mobility(int p0, int p, bitboard_t tss)
{
    const int AdjustCount[][15] = {
        {-3, -2, -1, 0, 1, 2, 3, 4, 4},
        {-4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 5, 6, 6, 7},
        {-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 6, 7, 7}
    };
    const eval_t Weight[] = {{8, 8}, {10, 10}, {4, 8}, {2, 4}};

    const int cnt = AdjustCount[p0][bb::count(tss)];
    return Weight[p] * cnt;
}

eval_t mobility(const Position& pos, Color us, bitboard_t attacks[NB_COLOR][NB_PIECE])
{
    bitboard_t fss, tss, occ;
    Square from;
    Piece piece;

    eval_t result = {0, 0};

    attacks[us][KING] = bb::kattacks(pos.king_square(us));
    attacks[~us][PAWN] = pawn_attacks(pos, ~us);

    for (piece = KNIGHT; piece <= QUEEN; ++piece)
        attacks[us][piece] = 0;

    const bitboard_t targets = ~(pos.occ(us, KING, PAWN) | attacks[~us][PAWN]);

    // Knight mobility
    fss = pos.occ(us, KNIGHT);

    while (fss) {
        tss = bb::nattacks(bb::pop_lsb(fss));
        attacks[us][KNIGHT] |= tss;
        result += score_mobility(KNIGHT, KNIGHT, tss & targets);
    }

    // Lateral mobility
    fss = pos.occ(us, ROOK, QUEEN);
    occ = pos.occ() ^ fss;    // RQ see through each other

    while (fss) {
        tss = bb::rattacks(from = bb::pop_lsb(fss), occ);
        attacks[us][piece = pos.piece_on(from)] |= tss;
        result += score_mobility(ROOK, pos.piece_on(from), tss & targets);
    }

    // Diagonal mobility
    fss = pos.occ(us, BISHOP, QUEEN);
    occ = pos.occ() ^ fss;    // BQ see through each other

    while (fss) {
        tss = bb::battacks(from = bb::pop_lsb(fss), occ);
        attacks[us][piece = pos.piece_on(from)] |= tss;
        result += score_mobility(BISHOP, pos.piece_on(from), tss & targets);
    }

    return result;
}

eval_t bishop_pair(const Position& pos, Color us)
{
    // FIXME: verify that both B are indeed on different color squares
    return bb::several(pos.occ(us, BISHOP)) ? eval_t {80, 100} :
           eval_t {0, 0};
}

eval_t tactics(const Position& pos, Color us, bitboard_t attacks[NB_COLOR][NB_PIECE])
{
    static const int Hanging[] = {66, 66, 81, 150};
    int result = 0;
    bitboard_t b = (attacks[~us][PAWN] & (pos.occ(us) ^ pos.occ(us, PAWN)))
                   | (attacks[~us][KNIGHT] & pos.occ(us, ROOK, QUEEN))
                   | (attacks[~us][BISHOP] & pos.occ(us, ROOK));

    while (b) {
        const Piece p = pos.piece_on(bb::pop_lsb(b));
        assert(KNIGHT <= p && p <= QUEEN);
        result -= Hanging[p];
    }

    return eval_t{result, 0};
}

eval_t safety(const Position& pos, Color us, bitboard_t attacks[NB_COLOR][NB_PIECE])
{
    static const int AttackWeight[2] = {18, 30};
    const bitboard_t dangerZone = attacks[us][KING] & ~attacks[us][PAWN];
    const bitboard_t defendedByPieces = attacks[us][KNIGHT] | attacks[us][BISHOP]
                                        | attacks[us][ROOK] | attacks[us][QUEEN];
    int result = 0, cnt = 0;

    for (Piece p = KNIGHT; p <= QUEEN; ++p) {
        const bitboard_t attacked = attacks[~us][p] & dangerZone;

        if (attacked) {
            cnt++;
            result -= bb::count(attacked) * AttackWeight[p/2]
                      - bb::count(attacked & defendedByPieces) * AttackWeight[p/2] / 2;
        }
    }

    static const int CheckWeight = 70;
    const Square ks = pos.king_square(us);
    const bitboard_t checks[] = {
        bb::nattacks(ks) & attacks[~us][KNIGHT],
        bb::battacks(ks, pos.occ()) & attacks[~us][BISHOP],
        bb::rattacks(ks, pos.occ()) & attacks[~us][ROOK],
        (bb::battacks(ks, pos.occ()) | bb::rattacks(ks, pos.occ())) & attacks[~us][QUEEN]
    };

    for (Piece p = KNIGHT; p <= QUEEN; ++p)
        if (checks[p]) {
            const bitboard_t b = checks[p] & ~(pos.occ(~us) | attacks[us][PAWN] | attacks[us][KING]);

            if (b) {
                cnt++;
                result -= bb::count(b) * CheckWeight;
            }
        }

    return eval_t{result * (2 + cnt) / 4, 0};
}

eval_t pawns(const Position& pos, Color us)
{
    static const eval_t Isolated[2] = {{20, 40}, {40, 40}};

    eval_t result = {0, 0};
    bitboard_t b = pos.occ(us, PAWN);

    while (b) {
        const Square s = bb::pop_lsb(b);
        const File f = file_of(s);
        const bitboard_t adjacentFiles = (f > FILE_A ? bb::file(f - 1) : 0)
                                         | (f < FILE_H ? bb::file(f + 1) : 0);

        const bool isolated = !(adjacentFiles & pos.occ(us, PAWN));
        const bool exposed = !(bb::pawn_path(us, s) & pos.occ(PAWN));

        if (isolated)
            result -= Isolated[exposed];
    }

    return result;
}

int blend(const Position& pos, eval_t e)
{
    static const int full = 4 * (N + B + R) + 2 * Q;
    const int total = pos.piece_material().eg();
    return e.op() * total / full + e.eg() * (full - total) / full;
}

}    // namespace

int evaluate(const Position& pos)
{
    assert(!pos.checkers());
    eval_t e[NB_COLOR] = {pos.pst(), {0, 0}};

    bitboard_t attacks[NB_COLOR][NB_PIECE];

    // Mobility first, because it fills in the attacks[] array
    for (Color c = WHITE; c <= BLACK; ++c)
        e[c] += mobility(pos, c, attacks);

    for (Color c = WHITE; c <= BLACK; ++c) {
        e[c] += bishop_pair(pos, c);
        e[c] += tactics(pos, c, attacks);
        e[c] += safety(pos, c, attacks);
        e[c] += pawns(pos, c);
    }

    const Color us = pos.turn();
    return blend(pos, e[us] - e[~us]);
}
