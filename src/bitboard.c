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
#include "bitboard.h"

static const int PDir[2][2] = {{1,-1},{1,1}};
static const int NDir[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
static const int KDir[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};

bitboard_t PAttacks[NB_COLOR][NB_SQUARE];
bitboard_t NAttacks[NB_SQUARE];
bitboard_t KAttacks[NB_SQUARE];

bitboard_t BPseudoAttacks[NB_SQUARE];
bitboard_t RPseudoAttacks[NB_SQUARE];

bitboard_t Segment[NB_SQUARE][NB_SQUARE];
bitboard_t Ray[NB_SQUARE][NB_SQUARE];

static void safe_set_bit(bitboard_t *b, int r, int f)
{
    if (0 <= r && r < NB_RANK && 0 <= f && f < NB_FILE)
        bb_set(b, square(r, f));
}

static void init_leaper_attacks()
{
    for (int s = A1; s <= H8; s++) {
        const int r = rank_of(s), f = file_of(s);

        for (int d = 0; d < 8; d++) {
            safe_set_bit(&NAttacks[s], r + NDir[d][0], f + NDir[d][1]);
            safe_set_bit(&KAttacks[s], r + KDir[d][0], f + KDir[d][1]);
        }

        for (int d = 0; d < 2; d++) {
            safe_set_bit(&PAttacks[WHITE][s], r + PDir[d][0], f + PDir[d][1]);
            safe_set_bit(&PAttacks[BLACK][s], r - PDir[d][0], f - PDir[d][1]);
        }
    }
}

static void init_rays()
{
    for (int s1 = A1; s1 <= H8; s1++) {
        const int r1 = rank_of(s1), f1 = file_of(s1);

        for (int d = 0; d < 8; d++) {
            bitboard_t mask = 0;
            int r2 = r1, f2 = f1;

            while (0 <= r2 && r2 < NB_RANK && 0 <= f2 && f2 < NB_FILE) {
                const int s2 = square(r2, f2);
                bb_set(&mask, s2);
                Segment[s1][s2] = mask;
                r2 += KDir[d][0], f2 += KDir[d][1];
            }

            bitboard_t sqs = mask;

            while (sqs) {
                int s2 = bb_pop_lsb(&sqs);
                Ray[s1][s2] = mask;
            }
        }
    }
}

static const bitboard_t RMagic[NB_SQUARE] = {
    0x0080001020400080ull, 0x0040001000200040ull, 0x0080081000200080ull, 0x0080040800100080ull,
    0x0080020400080080ull, 0x0080010200040080ull, 0x0080008001000200ull, 0x0080002040800100ull,
    0x0000800020400080ull, 0x0000400020005000ull, 0x0000801000200080ull, 0x0000800800100080ull,
    0x0000800400080080ull, 0x0000800200040080ull, 0x0000800100020080ull, 0x0000800040800100ull,
    0x0000208000400080ull, 0x0000404000201000ull, 0x0000808010002000ull, 0x0000808008001000ull,
    0x0000808004000800ull, 0x0000808002000400ull, 0x0000010100020004ull, 0x0000020000408104ull,
    0x0000208080004000ull, 0x0000200040005000ull, 0x0000100080200080ull, 0x0000080080100080ull,
    0x0000040080080080ull, 0x0000020080040080ull, 0x0000010080800200ull, 0x0000800080004100ull,
    0x0000204000800080ull, 0x0000200040401000ull, 0x0000100080802000ull, 0x0000080080801000ull,
    0x0000040080800800ull, 0x0000020080800400ull, 0x0000020001010004ull, 0x0000800040800100ull,
    0x0000204000808000ull, 0x0000200040008080ull, 0x0000100020008080ull, 0x0000080010008080ull,
    0x0000040008008080ull, 0x0000020004008080ull, 0x0000010002008080ull, 0x0000004081020004ull,
    0x0000204000800080ull, 0x0000200040008080ull, 0x0000100020008080ull, 0x0000080010008080ull,
    0x0000040008008080ull, 0x0000020004008080ull, 0x0000800100020080ull, 0x0000800041000080ull,
    0x00FFFCDDFCED714Aull, 0x007FFCDDFCED714Aull, 0x003FFFCDFFD88096ull, 0x0000040810002101ull,
    0x0001000204080011ull, 0x0001000204000801ull, 0x0001000082000401ull, 0x0001FFFAABFAD1A2ull
};

static const bitboard_t BMagic[NB_SQUARE] = {
    0x0002020202020200ull, 0x0002020202020000ull, 0x0004010202000000ull, 0x0004040080000000ull,
    0x0001104000000000ull, 0x0000821040000000ull, 0x0000410410400000ull, 0x0000104104104000ull,
    0x0000040404040400ull, 0x0000020202020200ull, 0x0000040102020000ull, 0x0000040400800000ull,
    0x0000011040000000ull, 0x0000008210400000ull, 0x0000004104104000ull, 0x0000002082082000ull,
    0x0004000808080800ull, 0x0002000404040400ull, 0x0001000202020200ull, 0x0000800802004000ull,
    0x0000800400A00000ull, 0x0000200100884000ull, 0x0000400082082000ull, 0x0000200041041000ull,
    0x0002080010101000ull, 0x0001040008080800ull, 0x0000208004010400ull, 0x0000404004010200ull,
    0x0000840000802000ull, 0x0000404002011000ull, 0x0000808001041000ull, 0x0000404000820800ull,
    0x0001041000202000ull, 0x0000820800101000ull, 0x0000104400080800ull, 0x0000020080080080ull,
    0x0000404040040100ull, 0x0000808100020100ull, 0x0001010100020800ull, 0x0000808080010400ull,
    0x0000820820004000ull, 0x0000410410002000ull, 0x0000082088001000ull, 0x0000002011000800ull,
    0x0000080100400400ull, 0x0001010101000200ull, 0x0002020202000400ull, 0x0001010101000200ull,
    0x0000410410400000ull, 0x0000208208200000ull, 0x0000002084100000ull, 0x0000000020880000ull,
    0x0000001002020000ull, 0x0000040408020000ull, 0x0004040404040000ull, 0x0002020202020000ull,
    0x0000104104104000ull, 0x0000002082082000ull, 0x0000000020841000ull, 0x0000000000208800ull,
    0x0000000010020200ull, 0x0000000404080200ull, 0x0000040404040400ull, 0x0002020202020200ull
};

static bitboard_t RMagicDB[0x19000], BMagicDB[0x1480];

bitboard_t BMask[NB_SQUARE];
bitboard_t RMask[NB_SQUARE];

int BShift[NB_SQUARE];
int RShift[NB_SQUARE];

bitboard_t *BMagicArray[NB_SQUARE];
bitboard_t *RMagicArray[NB_SQUARE];

static bitboard_t calc_sliding_attacks(int s, bitboard_t occ, const int dir[4][2])
{
    const int r = rank_of(s), f = file_of(s);
    bitboard_t result = 0;

    for (int i = 0; i < 4; i++) {
        int dr = dir[i][0], df = dir[i][1];
        int _r = r + dr, _f = f + df;

        while (0 <= _r && _r < NB_RANK && 0 <= _f && _f < NB_FILE) {
            const int _sq = square(_r, _f);
            bb_set(&result, _sq);

            if (bb_test(occ, _sq))
                break;

            _r += dr, _f += df;
        }
    }

    return result;
}

static void do_slider_attacks(int s, bitboard_t mask[], const bitboard_t magic[], int shift[],
                        bitboard_t *magicArray[], const int dir[4][2])
{
    bitboard_t edges = ((bb_rank(RANK_1) | bb_rank(RANK_8)) & ~bb_rank(rank_of(s))) |
        ((bb_file(RANK_1) | bb_file(RANK_8)) & ~bb_file(file_of(s)));
    mask[s] = calc_sliding_attacks(s, 0, dir) & ~edges;

    shift[s] = 64 - bb_count(mask[s]);

    if (s < H8)
        magicArray[s + 1] = magicArray[s] + (1 << bb_count(mask[s]));

    // Use the Carry-Rippler trick to loop over the subsets of mask[s]
    bitboard_t occ = 0;

    do {
        magicArray[s][(occ * magic[s]) >> shift[s]] = calc_sliding_attacks(s, occ, dir);
        occ = (occ - mask[s]) & mask[s];
    } while (occ);
}

void init_slider_attacks()
{
    const int Bdir[4][2] = {{-1,-1}, {-1,1}, {1,-1}, {1,1}};
    const int Rdir[4][2] = {{-1,0}, {0,-1}, {0,1}, {1,0}};

    BMagicArray[0] = BMagicDB;
    RMagicArray[0] = RMagicDB;

    for (int s = A1; s <= H8; s++) {
        do_slider_attacks(s, BMask, BMagic, BShift, BMagicArray, Bdir);
        do_slider_attacks(s, RMask, RMagic, RShift, RMagicArray, Rdir);
    }
}

bitboard_t bb_battacks(int s, bitboard_t occ)
{
    BOUNDS(s, NB_SQUARE);
    return BMagicArray[s][((occ & BMask[s]) * BMagic[s]) >> BShift[s]];
}

bitboard_t bb_rattacks(int s, bitboard_t occ)
{
    BOUNDS(s, NB_SQUARE);
    return RMagicArray[s][((occ & RMask[s]) * RMagic[s]) >> RShift[s]];
}

static void init_slider_pseudo_attacks()
{
    for (int s = A1; s <= H8; s++) {
        BPseudoAttacks[s] = bb_battacks(s, 0);
        RPseudoAttacks[s] = bb_rattacks(s, 0);
    }
}

void bb_init()
{
    init_rays();
    init_leaper_attacks();
    init_slider_attacks();
    init_slider_pseudo_attacks();
}

/* Bitboard Accessors */

bitboard_t bb_rank(int r)
{
    BOUNDS(r, NB_RANK);
    return 0xFFULL << (8 * r);
}

bitboard_t bb_file(int f)
{
    BOUNDS(f, NB_FILE);
    return 0x0101010101010101ULL << f;
}

/* Bit manipulation */

bool bb_test(bitboard_t b, int s)
{
    BOUNDS(s, NB_SQUARE);
    return b & (1ULL << s);
}

void bb_clear(bitboard_t *b, int s)
{
    BOUNDS(s, NB_SQUARE);
    assert(bb_test(*b, s));
    *b ^= 1ULL << s;
}

void bb_set(bitboard_t *b, int s)
{
    BOUNDS(s, NB_SQUARE);
    assert(!bb_test(*b, s));
    *b ^= 1ULL << s;
}

bitboard_t bb_shift(bitboard_t b, int i)
{
    assert(-63 <= i && i <= 63);    // forbid oversized shift (undefined behaviour)
    return i > 0 ? b << i : b >> -i;
}

int bb_lsb(bitboard_t b)
{
    assert(b);
    return __builtin_ffsll(b) - 1;
}

int bb_msb(bitboard_t b)
{
    assert(b);
    return 63 - __builtin_clzll(b);
}

int bb_pop_lsb(bitboard_t *b)
{
    int s = bb_lsb(*b);
    *b &= *b - 1;
    return s;
}

bool bb_several(bitboard_t b)
{
    return b & (b - 1);
}

int bb_count(bitboard_t b)
{
    return __builtin_popcountll(b);
}

/* Debug print */

void bb_print(bitboard_t b)
{
    for (int r = RANK_8; r >= RANK_1; r--) {
        char line[] = ". . . . . . . .";

        for (int f = FILE_A; f <= FILE_H; f++) {
            if (bb_test(b, square(r, f)))
                line[2 * f] = 'X';
        }

        puts(line);
    }

    puts("");
}
