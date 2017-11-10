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

#ifdef PEXT
    #include <immintrin.h> // Header for _pext_u64() intrinsic
#endif

bitboard_t Rank[NB_RANK], File[NB_FILE];
bitboard_t PAttacks[NB_COLOR][NB_SQUARE], NAttacks[NB_SQUARE], KAttacks[NB_SQUARE];
bitboard_t BPseudoAttacks[NB_SQUARE], RPseudoAttacks[NB_SQUARE];
bitboard_t Segment[NB_SQUARE][NB_SQUARE], Ray[NB_SQUARE][NB_SQUARE];

static void safe_set_bit(bitboard_t *b, int r, int f)
{
    if (0 <= r && r < NB_RANK && 0 <= f && f < NB_FILE)
        bb_set(b, square(r, f));
}

static const bitboard_t RMagic[NB_SQUARE] = {
    0x808000645080c000ULL, 0x208020001480c000ULL, 0x4180100160008048ULL, 0x8180100018001680ULL,
    0x4200082010040201ULL, 0x8300220400010008ULL, 0x3100120000890004ULL, 0x4080004500012180ULL,
    0x1548000a1804008ULL, 0x4881004005208900ULL, 0x480802000801008ULL, 0x2e8808010008800ULL,
    0x8cd804800240080ULL, 0x8a058002008c0080ULL, 0x514000c480a1001ULL, 0x101000282004d00ULL,
    0x2048848000204000ULL, 0x3020088020804000ULL, 0x4806020020841240ULL, 0x6080420008102202ULL,
    0x10050011000800ULL, 0xac00808004000200ULL, 0x10100020004ULL, 0x1500020004004581ULL,
    0x4c00180052080ULL, 0x220028480254000ULL, 0x2101200580100080ULL, 0x407201200084200ULL,
    0x18004900100500ULL, 0x100200020008e410ULL, 0x81020400100811ULL, 0x12200024494ULL,
    0x8006c002808006a5ULL, 0x4201000404000ULL, 0x5402202001180ULL, 0x81001002100ULL,
    0x100801000500ULL, 0x4000020080800400ULL, 0x4005050214001008ULL, 0x810100118b000042ULL,
    0xd01020040820020ULL, 0x140a010014000ULL, 0x420001500210040ULL, 0x54210010030009ULL,
    0x4000408008080ULL, 0x2000400090100ULL, 0x840200010100ULL, 0x233442820004ULL,
    0x800a42002b008200ULL, 0x240200040009080ULL, 0x242001020408200ULL, 0x4000801000480480ULL,
    0x2288008044000880ULL, 0xa800400020180ULL, 0x30011002880c00ULL, 0x41110880440200ULL,
    0x2001100442082ULL, 0x1a0104002208101ULL, 0x80882014010200aULL, 0x100100600409ULL,
    0x2011048204402ULL, 0x12000168041002ULL, 0x80100008a000421ULL, 0x240022044031182ULL
};

static const bitboard_t BMagic[NB_SQUARE] = {
    0x88b030028800d040ULL, 0x18242044c008010ULL, 0x10008200440000ULL, 0x4311040888800a00ULL,
    0x1910400000410aULL, 0x2444240440000000ULL, 0xcd2080108090008ULL, 0x2048242410041004ULL,
    0x8884441064080180ULL, 0x42131420a0240ULL, 0x28882800408400ULL, 0x204384040b820200ULL,
    0x402040420800020ULL, 0x20910282304ULL, 0x96004b10082200ULL, 0x4000a44218410802ULL,
    0x808034002081241ULL, 0x101805210e1408ULL, 0x9020400208010220ULL, 0x820050c010044ULL,
    0x24005480a00000ULL, 0x200200900890ULL, 0x808040049c100808ULL, 0x9020202200820802ULL,
    0x410282124200400ULL, 0x90106008010110ULL, 0x8001100501004201ULL, 0x104080004030c10ULL,
    0x80840040802008ULL, 0x2008008102406000ULL, 0x2000888004040460ULL, 0xd0421242410410ULL,
    0x8410100401280800ULL, 0x801012000108428ULL, 0x402080300b04ULL, 0xc20020080480080ULL,
    0x40100e0201502008ULL, 0x4014208200448800ULL, 0x4050020607084501ULL, 0x1002820180020288ULL,
    0x800610040540a0c0ULL, 0x301009014081004ULL, 0x2200610040502800ULL, 0x300442011002800ULL,
    0x1022009002208ULL, 0x110011000202100ULL, 0x1464082204080240ULL, 0x21310205800200ULL,
    0x814020210040109ULL, 0xc102008208c200a0ULL, 0xc100702128080000ULL, 0x1044205040000ULL,
    0x1041002020000ULL, 0x4200040408021000ULL, 0x4004040c494000ULL, 0x2010108900408080ULL,
    0x820801040284ULL, 0x800004118111000ULL, 0x203040201108800ULL, 0x2504040804208803ULL,
    0x228000908030400ULL, 0x10402082020200ULL, 0xa0402208010100ULL, 0x30c0214202044104ULL
};

static bitboard_t RAttacks[0x19000], BAttacks[0x1480];
static bitboard_t *BAttacksPtr[NB_SQUARE], *RAttacksPtr[NB_SQUARE];

static bitboard_t BMask[NB_SQUARE], RMask[NB_SQUARE];
static unsigned BShift[NB_SQUARE], RShift[NB_SQUARE];

static bitboard_t slider_attacks(int s, bitboard_t occ, const int dir[4][2])
{
    bitboard_t result = 0;

    for (int i = 0; i < 4; i++) {
        int dr = dir[i][0], df = dir[i][1];
        int r, f;

        for (r = rank_of(s) + dr, f = file_of(s) + df;
                (unsigned)r < NB_RANK && (unsigned)f < NB_FILE;
                r += dr, f += df) {
            const int sq = square(r, f);
            bb_set(&result, sq);

            if (bb_test(occ, sq))
                break;
        }
    }

    return result;
}

static int slider_index(bitboard_t occ, bitboard_t mask, bitboard_t magic, unsigned shift)
{
#ifdef PEXT
    (void)magic, (void)shift;  // Silence compiler warnings (unused variables)
    return _pext_u64(occ, mask);
#else
    return ((occ & mask) * magic) >> shift;
#endif
}

static void init_slider_attacks(int s, bitboard_t mask[NB_SQUARE], const bitboard_t magic[NB_SQUARE],
    unsigned shift[NB_SQUARE], bitboard_t *attacksPtr[NB_SQUARE], const int dir[4][2])
{
    bitboard_t edges = ((Rank[RANK_1] | Rank[RANK_8]) & ~Rank[rank_of(s)]) |
        ((File[RANK_1] | File[RANK_8]) & ~File[file_of(s)]);
    mask[s] = slider_attacks(s, 0, dir) & ~edges;
    shift[s] = 64 - bb_count(mask[s]);

    if (s < H8)
        attacksPtr[s + 1] = attacksPtr[s] + (1 << bb_count(mask[s]));

    // Use the Carry-Rippler trick to loop over the subsets of mask[s]
    bitboard_t occ = 0;

    do {
        attacksPtr[s][slider_index(occ, mask[s], magic[s], shift[s])] = slider_attacks(s, occ, dir);
        occ = (occ - mask[s]) & mask[s];
    } while (occ);
}

void bb_init()
{
    const int PDir[2][2] = {{1,-1}, {1,1}};
    const int NDir[8][2] = {{-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}};
    const int KDir[8][2] = {{-1,-1}, {-1,0}, {-1,1}, {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}};
    const int Bdir[4][2] = {{-1,-1}, {-1,1}, {1,-1}, {1,1}};
    const int Rdir[4][2] = {{-1,0}, {0,-1}, {0,1}, {1,0}};

    // Initialise Rank[] and File[]
    for (int i = 0; i < 8; i++) {
        Rank[i] = 0xFFULL << (8 * i);
        File[i] = 0x0101010101010101ULL << i;
    }

    // Initialise Ray[][] and Segment[][]
    for (int s = A1; s <= H8; s++) {
        for (int d = 0; d < 8; d++) {
            bitboard_t mask = 0;
            int r2 = rank_of(s), f2 = file_of(s);

            while (0 <= r2 && r2 < NB_RANK && 0 <= f2 && f2 < NB_FILE) {
                const int s2 = square(r2, f2);
                bb_set(&mask, s2);
                Segment[s][s2] = mask;
                r2 += KDir[d][0], f2 += KDir[d][1];
            }

            bitboard_t sqs = mask;

            while (sqs)
                Ray[s][bb_pop_lsb(&sqs)] = mask;
        }
    }

    // Initialise leaper attacks (N, K, P)
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

    // Initialise slider attacks (B, R)
    BAttacksPtr[0] = BAttacks;
    RAttacksPtr[0] = RAttacks;

    for (int s = A1; s <= H8; s++) {
        init_slider_attacks(s, BMask, BMagic, BShift, BAttacksPtr, Bdir);
        init_slider_attacks(s, RMask, RMagic, RShift, RAttacksPtr, Rdir);

        BPseudoAttacks[s] = BAttacksPtr[s][0];
        RPseudoAttacks[s] = RAttacksPtr[s][0];
    }
}

bitboard_t bb_battacks(int s, bitboard_t occ)
{
    BOUNDS(s, NB_SQUARE);
    return BAttacksPtr[s][slider_index(occ, BMask[s], BMagic[s], BShift[s])];
}

bitboard_t bb_rattacks(int s, bitboard_t occ)
{
    BOUNDS(s, NB_SQUARE);
    return RAttacksPtr[s][slider_index(occ, RMask[s], RMagic[s], RShift[s])];
}

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
    assert(-63 <= i && i <= 63);  // forbid oversized shift (undefined behaviour)
    return i > 0 ? b << i : b >> -i;
}

int bb_lsb(bitboard_t b)
{
    assert(b);
    return __builtin_ctzll(b);
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
