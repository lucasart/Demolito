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

thread_local PawnEntry PawnHash[NB_PAWN_ENTRY];

namespace {

bitboard_t pawn_attacks(const Position& pos, Color c)
{
    const bitboard_t pawns = pieces(pos, c, PAWN);
    return bb::shift(pawns & ~bb::file(FILE_A), push_inc(c) + LEFT)
           | bb::shift(pawns & ~bb::file(FILE_H), push_inc(c) + RIGHT);
}

eval_t score_mobility(int p0, int p, bitboard_t tss)
{
    assert(KNIGHT <= p0 && p0 <= ROOK);
    assert(KNIGHT <= p && p <= QUEEN);

    static const int AdjustCount[ROOK+1][15] = {
        {-3, -2, -1, 0, 1, 2, 3, 4, 4},
        {-4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 5, 6, 6, 7},
        {-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 6, 7, 7}
    };
    static const eval_t Weight[] = {{8, 8}, {10, 10}, {4, 8}, {2, 4}};

    return Weight[p] * AdjustCount[p0][bb::count(tss)];
}

eval_t mobility(const Position& pos, Color us, bitboard_t attacks[NB_COLOR][NB_PIECE+1])
{
    bitboard_t fss, tss, occ;
    Square from;
    Piece piece;

    eval_t result = {0, 0};

    attacks[us][KING] = bb::kattacks(king_square(pos, us));
    attacks[~us][PAWN] = pawn_attacks(pos, ~us);

    for (piece = KNIGHT; piece <= QUEEN; ++piece)
        attacks[us][piece] = 0;

    const bitboard_t targets = ~(pieces(pos, us, KING, PAWN) | attacks[~us][PAWN]);

    // Knight mobility
    fss = pieces(pos, us, KNIGHT);

    while (fss) {
        tss = bb::nattacks(bb::pop_lsb(fss));
        attacks[us][KNIGHT] |= tss;
        result += score_mobility(KNIGHT, KNIGHT, tss & targets);
    }

    // Lateral mobility
    fss = pieces(pos, us, ROOK, QUEEN);
    occ = pieces(pos) ^ fss;    // RQ see through each other

    while (fss) {
        tss = bb::rattacks(from = bb::pop_lsb(fss), occ);
        attacks[us][piece = pos.piece_on(from)] |= tss;
        result += score_mobility(ROOK, pos.piece_on(from), tss & targets);
    }

    // Diagonal mobility
    fss = pieces(pos, us, BISHOP, QUEEN);
    occ = pieces(pos) ^ fss;    // BQ see through each other

    while (fss) {
        tss = bb::battacks(from = bb::pop_lsb(fss), occ);
        attacks[us][piece = pos.piece_on(from)] |= tss;
        result += score_mobility(BISHOP, pos.piece_on(from), tss & targets);
    }

    attacks[us][NB_PIECE] = attacks[us][KNIGHT] | attacks[us][BISHOP] | attacks[us][ROOK] |
                            attacks[us][QUEEN];

    return result;
}

eval_t bishop_pair(const Position& pos, Color us)
{
    // FIXME: verify that both B are indeed on different color squares
    return bb::several(pieces(pos, us, BISHOP)) ? eval_t{102, 114} :
           eval_t{0, 0};
}

eval_t tactics(const Position& pos, Color us, bitboard_t attacks[NB_COLOR][NB_PIECE+1])
{
    static const int Hanging[QUEEN+1] = {66, 66, 81, 150};
    int result = 0;
    bitboard_t b = (attacks[~us][PAWN] & (pos.occ(us) ^ pieces(pos, us, PAWN)))
                   | (attacks[~us][KNIGHT] & pieces(pos, us, ROOK, QUEEN))
                   | (attacks[~us][BISHOP] & pieces(pos, us, ROOK));

    while (b) {
        const Piece p = pos.piece_on(bb::pop_lsb(b));
        assert(KNIGHT <= p && p <= QUEEN);
        result -= Hanging[p];
    }

    return eval_t{result, 0};
}

eval_t safety(const Position& pos, Color us, bitboard_t attacks[NB_COLOR][NB_PIECE+1])
{
    static const int AttackWeight[2] = {18, 30};
    const bitboard_t dangerZone = attacks[us][KING] & ~attacks[us][PAWN];
    int result = 0, cnt = 0;

    for (Piece p = KNIGHT; p <= QUEEN; ++p) {
        const bitboard_t attacked = attacks[~us][p] & dangerZone;

        if (attacked) {
            cnt++;
            result -= bb::count(attacked) * AttackWeight[p/2]
                      - bb::count(attacked & attacks[us][NB_PIECE]) * AttackWeight[p/2] / 2;
        }
    }

    static const int CheckWeight = 70;
    const Square ks = king_square(pos, us);
    const bitboard_t checks[QUEEN+1] = {
        bb::nattacks(ks) & attacks[~us][KNIGHT],
        bb::battacks(ks, pieces(pos)) & attacks[~us][BISHOP],
        bb::rattacks(ks, pieces(pos)) & attacks[~us][ROOK],
        (bb::battacks(ks, pieces(pos)) | bb::rattacks(ks, pieces(pos))) & attacks[~us][QUEEN]
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

eval_t passer(Color us, Square pawn, Square ourKing, Square theirKing)
{
    const Rank r = relative_rank(us, pawn);
    const int L = r - RANK_2;
    const int Q = L * (L - 1);

    // score based on rank
    eval_t result = {11 * Q, 6 * (Q + L + 1)};

    // king distance adjustment
    if (Q) {
        const Square stop = pawn + push_inc(us);
        result.eg() += bb::king_distance(stop, theirKing) * 6 * Q;
        result.eg() -= bb::king_distance(stop, ourKing) * 3 * Q;
    }

    return result;
}

eval_t do_pawns(const Position& pos, Color us, bitboard_t attacks[NB_COLOR][NB_PIECE+1])
{
    static const eval_t Isolated[2] = {{20, 40}, {40, 40}};
    static const eval_t Hole[2] = {{16, 20}, {32, 20}};

    const bitboard_t ourPawns = pieces(pos, us, PAWN);
    const bitboard_t theirPawns = pieces(pos, ~us, PAWN);
    const Square ourKing = king_square(pos, us);
    const Square theirKing = king_square(pos, ~us);

    eval_t result = {0, 0};
    bitboard_t b = ourPawns;

    while (b) {
        const Square s = bb::pop_lsb(b);
        const Square stop = s + push_inc(us);
        const Rank r = rank_of(s);
        const File f = file_of(s);

        const bitboard_t adjacentFiles = bb::adjacent_files(f);
        const bitboard_t besides = ourPawns & adjacentFiles;

        const bool chained = besides & (bb::rank(r) | bb::rank(us == WHITE ? r - 1 : r + 1));
        const bool hole = !(bb::pawn_span(~us, stop) & ourPawns) && bb::test(attacks[~us][PAWN], stop);
        const bool isolated = !(adjacentFiles & ourPawns);
        const bool exposed = !(bb::pawn_path(us, s) & pos.occ(PAWN));
        const bool passed = exposed && !(bb::pawn_span(us, s) & theirPawns);

        if (chained) {
            const int rr = relative_rank(us, r) - RANK_2;
            const bool support = ourPawns & bb::pattacks(~us, stop);
            const int bonus = rr * (rr + support) * 3;
            result += {8 + bonus / 2, bonus};
        } else if (hole)
            result -= Hole[exposed];
        else if (isolated)
            result -= Isolated[exposed];

        if (passed)
            result += passer(us, s, ourKing, theirKing);
    }

    return result;
}

eval_t pawns(const Position& pos, bitboard_t attacks[NB_COLOR][NB_PIECE+1])
// Pawn evaluation is directly a diff, from white's pov. This reduces by half the
// size of the pawn hash table.
{
    const uint64_t key = pos.pawn_key();
    const size_t idx = key & (NB_PAWN_ENTRY - 1);

    if (PawnHash[idx].key == key)
        return PawnHash[idx].eval;

    PawnHash[idx].key = key;
    PawnHash[idx].eval = do_pawns(pos, WHITE, attacks) - do_pawns(pos, BLACK, attacks);
    return PawnHash[idx].eval;
}

int blend(const Position& pos, eval_t e)
{
    static const int full = 4 * (N + B + R) + 2 * Q;
    const int total = (pos.piece_material(WHITE) + pos.piece_material(BLACK)).eg();
    return e.op() * total / full + e.eg() * (full - total) / full;
}

}    // namespace

int evaluate(const Position& pos)
{
    assert(!pos.checkers());
    eval_t e[NB_COLOR] = {pos.pst(), {0, 0}};

    bitboard_t attacks[NB_COLOR][NB_PIECE+1];

    // Mobility first, because it fills in the attacks array
    for (Color c = WHITE; c <= BLACK; ++c)
        e[c] += mobility(pos, c, attacks);

    for (Color c = WHITE; c <= BLACK; ++c) {
        e[c] += bishop_pair(pos, c);
        e[c] += tactics(pos, c, attacks);
        e[c] += safety(pos, c, attacks);
    }

    e[WHITE] += pawns(pos, attacks);

    const Color us = pos.turn();
    return blend(pos, e[us] - e[~us]);
}
