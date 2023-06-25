/*
 * Demolito, a UCI chess engine. Copyright 2015-2020 lucasart.
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
#include "util.h"

#ifdef PEXT
    #include <immintrin.h> // Header for _pext_u64() intrinsic
#endif

bitboard_t Rank[NB_RANK], File[NB_FILE];
bitboard_t PawnAttacks[NB_COLOR][NB_SQUARE], KnightAttacks[NB_SQUARE], KingAttacks[NB_SQUARE];
bitboard_t Segment[NB_SQUARE][NB_SQUARE], Ray[NB_SQUARE][NB_SQUARE];

static void safe_set_bit(bitboard_t *b, int rank, int file) {
    if (0 <= rank && rank < NB_RANK && 0 <= file && file < NB_FILE)
        bb_set(b, square_from(rank, file));
}

static const bitboard_t RookMagic[NB_SQUARE] = {
    0x808000645080c000, 0x208020001480c000, 0x4180100160008048, 0x8180100018001680,
    0x4200082010040201, 0x8300220400010008, 0x3100120000890004, 0x4080004500012180,
    0x1548000a1804008,  0x4881004005208900, 0x480802000801008,  0x2e8808010008800,
    0x8cd804800240080,  0x8a058002008c0080, 0x514000c480a1001,  0x101000282004d00,
    0x2048848000204000, 0x3020088020804000, 0x4806020020841240, 0x6080420008102202,
    0x10050011000800,   0xac00808004000200, 0x10100020004,      0x1500020004004581,
    0x4c00180052080,    0x220028480254000,  0x2101200580100080, 0x407201200084200,
    0x18004900100500,   0x100200020008e410, 0x81020400100811,   0x12200024494,
    0x8006c002808006a5, 0x4201000404000,    0x5402202001180,    0x81001002100,
    0x100801000500,     0x4000020080800400, 0x4005050214001008, 0x810100118b000042,
    0xd01020040820020,  0x140a010014000,    0x420001500210040,  0x54210010030009,
    0x4000408008080,    0x2000400090100,    0x840200010100,     0x233442820004,
    0x800a42002b008200, 0x240200040009080,  0x242001020408200,  0x4000801000480480,
    0x2288008044000880, 0xa800400020180,    0x30011002880c00,   0x41110880440200,
    0x2001100442082,    0x1a0104002208101,  0x80882014010200a,  0x100100600409,
    0x2011048204402,    0x12000168041002,   0x80100008a000421,  0x240022044031182};

static const bitboard_t BishopMagic[NB_SQUARE] = {
    0x88b030028800d040, 0x18242044c008010,  0x10008200440000,   0x4311040888800a00,
    0x1910400000410a,   0x2444240440000000, 0xcd2080108090008,  0x2048242410041004,
    0x8884441064080180, 0x42131420a0240,    0x28882800408400,   0x204384040b820200,
    0x402040420800020,  0x20910282304,      0x96004b10082200,   0x4000a44218410802,
    0x808034002081241,  0x101805210e1408,   0x9020400208010220, 0x820050c010044,
    0x24005480a00000,   0x200200900890,     0x808040049c100808, 0x9020202200820802,
    0x410282124200400,  0x90106008010110,   0x8001100501004201, 0x104080004030c10,
    0x80840040802008,   0x2008008102406000, 0x2000888004040460, 0xd0421242410410,
    0x8410100401280800, 0x801012000108428,  0x402080300b04,     0xc20020080480080,
    0x40100e0201502008, 0x4014208200448800, 0x4050020607084501, 0x1002820180020288,
    0x800610040540a0c0, 0x301009014081004,  0x2200610040502800, 0x300442011002800,
    0x1022009002208,    0x110011000202100,  0x1464082204080240, 0x21310205800200,
    0x814020210040109,  0xc102008208c200a0, 0xc100702128080000, 0x1044205040000,
    0x1041002020000,    0x4200040408021000, 0x4004040c494000,   0x2010108900408080,
    0x820801040284,     0x800004118111000,  0x203040201108800,  0x2504040804208803,
    0x228000908030400,  0x10402082020200,   0xa0402208010100,   0x30c0214202044104};

static bitboard_t RookDB[0x19000], BishopDB[0x1480];
static bitboard_t *BishopAttacks[NB_SQUARE], *RookAttacks[NB_SQUARE];

static bitboard_t BishopMask[NB_SQUARE], RookMask[NB_SQUARE];
static unsigned BishopShift[NB_SQUARE], RookShift[NB_SQUARE];

// Compute (from scratch) the squares attacked by a sliding piece, moving in directions dir, given
// board occupancy occ.
static bitboard_t slider_attacks(int square, bitboard_t occ, const int dir[4][2]) {
    bitboard_t result = 0;

    for (int i = 0; i < 4; i++) {
        int dr = dir[i][0], df = dir[i][1];

        for (int rank = rank_of(square) + dr, file = file_of(square) + df;
             0 <= rank && rank < NB_RANK && 0 <= file && file < NB_FILE; rank += dr, file += df) {
            const int sq = square_from(rank, file);
            bb_set(&result, sq);

            if (bb_test(occ, sq))
                break;
        }
    }

    return result;
}

static unsigned slider_index(bitboard_t occ, bitboard_t mask, bitboard_t magic, unsigned shift) {
#ifdef PEXT
    (void)magic, (void)shift; // Silence compiler warnings (unused variables)
    return _pext_u64(occ, mask);
#else
    return (unsigned)(((occ & mask) * magic) >> shift);
#endif
}

static void init_slider_attacks(int square, bitboard_t mask[NB_SQUARE],
                                const bitboard_t magic[NB_SQUARE], unsigned shift[NB_SQUARE],
                                bitboard_t *attacksPtr[NB_SQUARE], const int dir[4][2]) {
    bitboard_t edges = ((Rank[RANK_1] | Rank[RANK_8]) & ~Rank[rank_of(square)]) |
                       ((File[RANK_1] | File[RANK_8]) & ~File[file_of(square)]);
    mask[square] = slider_attacks(square, 0, dir) & ~edges;
    shift[square] = (unsigned)(64 - bb_count(mask[square]));

    if (square < H8)
        attacksPtr[square + 1] = attacksPtr[square] + (1 << bb_count(mask[square]));

    // Loop over the subsets of mask[square]
    bitboard_t occ = 0;
    do {
        attacksPtr[square][slider_index(occ, mask[square], magic[square], shift[square])] =
            slider_attacks(square, occ, dir);
        occ = (occ - mask[square]) & mask[square]; // Carry-Rippler trick
    } while (occ);
}

static __attribute__((constructor)) void bb_init(void) {
    static const int PawnDir[2][2] = {{1, -1}, {1, 1}};
    static const int KnightDir[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                                        {1, -2},  {1, 2},  {2, -1},  {2, 1}};
    static const int KingDir[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                      {0, 1},   {1, -1}, {1, 0},  {1, 1}};
    static const int BishopDir[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    static const int RookDir[4][2] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}};

    // Initialise Rank[] and File[]
    for (int i = 0; i < 8; i++) {
        Rank[i] = 0xffULL << (8 * i);
        File[i] = 0x0101010101010101u << i;
    }

    // Initialise Ray[][] and Segment[][]
    for (int square = A1; square <= H8; square++) {
        for (int d = 0; d < 8; d++) {
            bitboard_t mask = 0;

            for (int r = rank_of(square), f = file_of(square);
                 0 <= r && r < NB_RANK && 0 <= f && f < NB_FILE;
                 r += KingDir[d][0], f += KingDir[d][1]) {
                const int s = square_from(r, f);
                bb_set(&mask, s);
                Segment[square][s] = mask;
            }

            bitboard_t sqs = mask;

            while (sqs)
                Ray[square][bb_pop_lsb(&sqs)] = mask;
        }
    }

    // Initialise leaper attacks (N, K, P)
    for (int square = A1; square <= H8; square++) {
        const int rank = rank_of(square), file = file_of(square);

        for (int d = 0; d < 8; d++) {
            safe_set_bit(&KnightAttacks[square], rank + KnightDir[d][0], file + KnightDir[d][1]);
            safe_set_bit(&KingAttacks[square], rank + KingDir[d][0], file + KingDir[d][1]);
        }

        for (int d = 0; d < 2; d++) {
            safe_set_bit(&PawnAttacks[WHITE][square], rank + PawnDir[d][0], file + PawnDir[d][1]);
            safe_set_bit(&PawnAttacks[BLACK][square], rank - PawnDir[d][0], file - PawnDir[d][1]);
        }
    }

    // Initialise slider attacks (B, R)
    BishopAttacks[0] = BishopDB;
    RookAttacks[0] = RookDB;

    for (int square = A1; square <= H8; square++) {
        init_slider_attacks(square, BishopMask, BishopMagic, BishopShift, BishopAttacks, BishopDir);
        init_slider_attacks(square, RookMask, RookMagic, RookShift, RookAttacks, RookDir);
    }

    // Validate all the precalculated bitboards in debug mode
    uint64_t h = 0;
    hash_blocks(Rank, sizeof Rank, &h);
    hash_blocks(File, sizeof File, &h);
    hash_blocks(Ray, sizeof Ray, &h);
    hash_blocks(Segment, sizeof Segment, &h);
    hash_blocks(KnightAttacks, sizeof KnightAttacks, &h);
    hash_blocks(KingAttacks, sizeof KingAttacks, &h);
    hash_blocks(PawnAttacks, sizeof PawnAttacks, &h);
    hash_blocks(BishopMask, sizeof BishopMask, &h);
    hash_blocks(RookMask, sizeof RookMask, &h);
    hash_blocks(BishopMagic, sizeof BishopMagic, &h);
    hash_blocks(RookMagic, sizeof RookMagic, &h);
    hash_blocks(BishopShift, sizeof BishopShift, &h);
    hash_blocks(RookShift, sizeof RookShift, &h);
    hash_blocks(BishopDB, sizeof BishopDB, &h);
    hash_blocks(RookDB, sizeof RookDB, &h);
#ifdef PEXT
    assert(h == 0x3ed5127a23c33053);
#else
    assert(h == 0x18b55a1336e2c557);
#endif
}

bitboard_t bb_bishop_attacks(int square, bitboard_t occ) {
    BOUNDS(square, NB_SQUARE);
    return BishopAttacks[square][slider_index(occ, BishopMask[square], BishopMagic[square],
                                              BishopShift[square])];
}

bitboard_t bb_rook_attacks(int square, bitboard_t occ) {
    BOUNDS(square, NB_SQUARE);
    return RookAttacks[square]
                      [slider_index(occ, RookMask[square], RookMagic[square], RookShift[square])];
}

bool bb_test(bitboard_t b, int square) {
    BOUNDS(square, NB_SQUARE);
    return b & (1ULL << square);
}

void bb_clear(bitboard_t *b, int square) {
    BOUNDS(square, NB_SQUARE);
    assert(bb_test(*b, square));
    *b ^= 1ULL << square;
}

void bb_set(bitboard_t *b, int square) {
    BOUNDS(square, NB_SQUARE);
    assert(!bb_test(*b, square));
    *b ^= 1ULL << square;
}

bitboard_t bb_shift(bitboard_t b, int i) {
    assert(-63 <= i && i <= 63); // oversized shift is undefined
    return i > 0 ? b << i : b >> -i;
}

int bb_lsb(bitboard_t b) {
    assert(b); // lsb(0) is undefined
    return __builtin_ctzll(b);
}

int bb_msb(bitboard_t b) {
    assert(b); // msb(0) is undefined
    return 63 - __builtin_clzll(b);
}

int bb_pop_lsb(bitboard_t *b) {
    int square = bb_lsb(*b);
    *b &= *b - 1;
    return square;
}

bool bb_several(bitboard_t b) { return b & (b - 1); }

int bb_count(bitboard_t b) { return __builtin_popcountll(b); }

void bb_print(bitboard_t b) {
    for (int rank = RANK_8; rank >= RANK_1; rank--) {
        char line[] = ". . . . . . . .";

        for (int file = FILE_A; file <= FILE_H; file++) {
            if (bb_test(b, square_from(rank, file)))
                line[2 * file] = 'X';
        }

        puts(line);
    }

    puts("");
}
