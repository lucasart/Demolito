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
#include "bitboard.h"

namespace {

const int PDir[2][2] = {{1,-1},{1,1}};
const int NDir[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
const int KDir[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};

bitboard_t PAttacks[NB_COLOR][NB_SQUARE];
bitboard_t NAttacks[NB_SQUARE];
bitboard_t KAttacks[NB_SQUARE];

bitboard_t BPseudoAttacks[NB_SQUARE];
bitboard_t RPseudoAttacks[NB_SQUARE];

bitboard_t Segment[NB_SQUARE][NB_SQUARE];
bitboard_t Ray[NB_SQUARE][NB_SQUARE];
bitboard_t PawnSpan[NB_COLOR][NB_SQUARE];

void safe_set_bit(bitboard_t& b, Rank r, File f)
{
    if (0 <= r && r < NB_RANK && 0 <= f && f < NB_FILE)
        bb::set(b, square(r, f));
}

void init_leaper_attacks()
{
    for (int sq = A1; sq <= H8; ++sq) {
        const Rank r = rank_of(sq);
        const File f = file_of(sq);

        for (int d = 0; d < 8; d++) {
            safe_set_bit(NAttacks[sq], r + NDir[d][0], f + NDir[d][1]);
            safe_set_bit(KAttacks[sq], r + KDir[d][0], f + KDir[d][1]);
        }

        for (int d = 0; d < 2; d++) {
            safe_set_bit(PAttacks[WHITE][sq], r + PDir[d][0], f + PDir[d][1]);
            safe_set_bit(PAttacks[BLACK][sq], r - PDir[d][0], f - PDir[d][1]);
        }
    }

    for (int sq = H8; sq >= A1; sq--)
        PawnSpan[WHITE][sq] = (rank_of(sq) == RANK_8 ? 0 : PawnSpan[WHITE][sq + 8])
                              | PAttacks[WHITE][sq];

    for (int sq = A1; sq <= H8; sq++)
        PawnSpan[BLACK][sq] = (rank_of(sq) == RANK_1 ? 0 : PawnSpan[BLACK][sq - 8])
                              | PAttacks[BLACK][sq];
}

void init_rays()
{
    for (int sq1 = A1; sq1 <= H8; ++sq1) {
        const Rank r1 = rank_of(sq1);
        const File f1 = file_of(sq1);

        for (int d = 0; d < 8; d++) {
            bitboard_t mask = 0;
            Rank r2 = r1;
            File f2 = f1;

            while (0 <= r2 && r2 < NB_RANK && 0 <= f2 && f2 < NB_FILE) {
                const int sq2 = square(r2, f2);
                bb::set(mask, sq2);
                Segment[sq1][sq2] = mask;
                r2 += KDir[d][0], f2 += KDir[d][1];
            }

            bitboard_t sqs = mask;

            while (sqs) {
                int sq2 = bb::pop_lsb(sqs);
                Ray[sq1][sq2] = mask;
            }
        }
    }
}

void init_slider_pseudo_attacks()
{
    for (int sq = A1; sq <= H8; ++sq) {
        BPseudoAttacks[sq] = bb::battacks(sq, 0);
        RPseudoAttacks[sq] = bb::rattacks(sq, 0);
    }
}

}    // namespace

namespace bb {

void init_slider_attacks();    // in magic.cc

void init()
{
    init_rays();
    init_leaper_attacks();
    init_slider_attacks();
    init_slider_pseudo_attacks();
}

/* Bitboard Accessors */

bitboard_t rank(Rank r)
{
    return 0xFFULL << (8 * r);
}

bitboard_t file(File f)
{
    return 0x0101010101010101ULL << f;
}

bitboard_t relative_rank(Color c, Rank r)
{
    return rank(c == WHITE ? r : RANK_8 - r);
}

bitboard_t pattacks(Color c, int sq)
{
    BOUNDS(sq, NB_SQUARE);

    return PAttacks[c][sq];
}

bitboard_t nattacks(int sq)
{
    BOUNDS(sq, NB_SQUARE);

    return NAttacks[sq];
}

bitboard_t kattacks(int sq)
{
    BOUNDS(sq, NB_SQUARE);

    return KAttacks[sq];
}

bitboard_t bpattacks(int sq)
{
    BOUNDS(sq, NB_SQUARE);

    return BPseudoAttacks[sq];
}

bitboard_t rpattacks(int sq)
{
    BOUNDS(sq, NB_SQUARE);

    return RPseudoAttacks[sq];
}

bitboard_t segment(int sq1, int sq2)
{
    BOUNDS(sq1, NB_SQUARE);
    BOUNDS(sq2, NB_SQUARE);

    return Segment[sq1][sq2];
}

bitboard_t ray(int sq1, int sq2)
{
    BOUNDS(sq1, NB_SQUARE);
    BOUNDS(sq2, NB_SQUARE);
    assert(sq1 != sq2);    // Ray[sq][sq] is undefined

    return Ray[sq1][sq2];
}

bitboard_t pawn_span(Color c, int sq)
{
    BOUNDS(sq, NB_SQUARE);

    return PawnSpan[c][sq];
}

/* Bit manipulation */

bool test(bitboard_t b, int sq)
{
    BOUNDS(sq, NB_SQUARE);

    return b & (1ULL << sq);
}

void clear(bitboard_t& b, int sq)
{
    BOUNDS(sq, NB_SQUARE);
    assert(test(b, sq));

    b ^= 1ULL << sq;
}

void set(bitboard_t& b, int sq)
{
    BOUNDS(sq, NB_SQUARE);
    assert(!test(b, sq));

    b ^= 1ULL << sq;
}

bitboard_t shift(bitboard_t b, int i)
{
    assert(-63 <= i && i <= 63);    // forbid oversized shift (undefined behaviour)

    return i > 0 ? b << i : b >> -i;
}

int lsb(bitboard_t b)
{
    assert(b);
    return __builtin_ffsll(b) - 1;
}

int msb(bitboard_t b)
{
    assert(b);
    return 63 - __builtin_clzll(b);
}

int pop_lsb(bitboard_t& b)
{
    int sq = lsb(b);
    b &= b - 1;
    return sq;
}

bool several(bitboard_t b)
{
    return b & (b - 1);
}

int count(bitboard_t b)
{
    return __builtin_popcountll(b);
}

/* Debug print */

void print(bitboard_t b)
{
    for (Rank r = RANK_8; r >= RANK_1; --r) {
        char line[] = ". . . . . . . .";

        for (File f = FILE_A; f <= FILE_H; ++f) {
            if (test(b, square(r, f)))
                line[2 * f] = 'X';
        }

        std::cout << line << '\n';
    }

    std::cout << std::endl;
}

}    // namespace bb
