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

/* Pre-calculated in eval_init() */

static bitboard_t PawnSpan[NB_COLOR][NB_SQUARE];
static bitboard_t PawnPath[NB_COLOR][NB_SQUARE];
static bitboard_t AdjacentFiles[NB_FILE];
static int KingDistance[NB_SQUARE][NB_SQUARE];

static bitboard_t pawn_attacks(const Position *pos, int c)
{
    const bitboard_t pawns = pos_pieces_cp(pos, c, PAWN);
    return bb_shift(pawns & ~bb_file(FILE_A), push_inc(c) + LEFT)
           | bb_shift(pawns & ~bb_file(FILE_H), push_inc(c) + RIGHT);
}

static eval_t score_mobility(int p0, int p, bitboard_t tss)
{
    assert(KNIGHT <= p0 && p0 <= ROOK);
    assert(KNIGHT <= p && p <= QUEEN);

    static const int AdjustCount[][15] = {
        {-4, -2, -1, 0, 1, 2, 3, 4, 4},
        {-5, -3, -2, -1, 0, 1, 2, 3, 4, 5, 5, 6, 6, 7},
        {-6, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 6, 7, 7}
    };
    static const eval_t Weight[] = {{6, 10}, {11, 12}, {6, 6}, {4, 6}};

    const int c = AdjustCount[p0][bb_count(tss)];
    return (eval_t) {Weight[p].op * c, Weight[p].eg * c};
}

static eval_t mobility(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
{
    bitboard_t fss, tss, occ;
    int from, piece;

    const int them = opposite(us);
    eval_t result = {0, 0};

    attacks[us][KING] = KAttacks[pos_king_square(pos, us)];
    attacks[them][PAWN] = pawn_attacks(pos, them);

    for (piece = KNIGHT; piece <= QUEEN; ++piece)
        attacks[us][piece] = 0;

    const bitboard_t targets = ~(pos_pieces_cpp(pos, us, KING, PAWN) | attacks[them][PAWN]);

    // Knight mobility
    fss = pos_pieces_cp(pos, us, KNIGHT);

    while (fss) {
        tss = NAttacks[bb_pop_lsb(&fss)];
        attacks[us][KNIGHT] |= tss;
        eval_add(&result, score_mobility(KNIGHT, KNIGHT, tss & targets));
    }

    // Lateral mobility
    fss = pos_pieces_cpp(pos, us, ROOK, QUEEN);
    occ = pos_pieces(pos) ^ fss;    // RQ see through each other

    while (fss) {
        tss = bb_rattacks(from = bb_pop_lsb(&fss), occ);
        attacks[us][piece = pos->pieceOn[from]] |= tss;
        eval_add(&result, score_mobility(ROOK, pos->pieceOn[from], tss & targets));
    }

    // Diagonal mobility
    fss = pos_pieces_cpp(pos, us, BISHOP, QUEEN);
    occ = pos_pieces(pos) ^ fss;    // BQ see through each other

    while (fss) {
        tss = bb_battacks(from = bb_pop_lsb(&fss), occ);
        attacks[us][piece = pos->pieceOn[from]] |= tss;
        eval_add(&result, score_mobility(BISHOP, pos->pieceOn[from], tss & targets));
    }

    attacks[us][NB_PIECE] = attacks[us][KNIGHT] | attacks[us][BISHOP] | attacks[us][ROOK] |
                            attacks[us][QUEEN];

    return result;
}

static eval_t bishop_pair(const Position *pos, int us)
{
    static const bitboard_t WhiteSquares = 0x55AA55AA55AA55AAULL;
    static const eval_t bonus = {102, 114};

    const bitboard_t bishops = pos_pieces_cp(pos, us, BISHOP);

    return (bishops & WhiteSquares) && (bishops & ~WhiteSquares) ? bonus : (eval_t) {0, 0};
}

static int tactics(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
{
    static const int Hanging[] = {66, 66, 81, 130};

    const int them = opposite(us);
    bitboard_t b = attacks[them][PAWN] & (pos->byColor[us] ^ pos_pieces_cp(pos, us, PAWN));
    b |= (attacks[them][KNIGHT] | attacks[them][BISHOP]) & pos_pieces_cpp(pos, us, ROOK, QUEEN);

    int result = 0;

    while (b) {
        const int p = pos->pieceOn[bb_pop_lsb(&b)];
        assert(KNIGHT <= p && p <= QUEEN);
        result -= Hanging[p];
    }

    return result;
}

static int safety(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
{
    static const int AttackWeight[2] = {43, 61};
    static const int CheckWeight = 63;

    const int them = opposite(us);
    int result = 0, cnt = 0;

    // Attacks around the King

    const bitboard_t dangerZone = attacks[us][KING] & ~attacks[us][PAWN];

    for (int p = KNIGHT; p <= QUEEN; ++p) {
        const bitboard_t attacked = attacks[them][p] & dangerZone;

        if (attacked) {
            cnt++;
            result -= bb_count(attacked) * AttackWeight[p / 2]
                      - bb_count(attacked & attacks[us][NB_PIECE]) * AttackWeight[p / 2] / 2;
        }
    }

    // Check threats

    const int ks = pos_king_square(pos, us);
    const bitboard_t occ = pos_pieces(pos);
    const bitboard_t checks[] = {
        NAttacks[ks] & attacks[them][KNIGHT],
        bb_battacks(ks, occ) & attacks[them][BISHOP],
        bb_rattacks(ks, occ) & attacks[them][ROOK],
        (bb_battacks(ks, occ) | bb_rattacks(ks, occ)) & attacks[them][QUEEN]
    };

    for (int p = KNIGHT; p <= QUEEN; ++p)
        if (checks[p]) {
            const bitboard_t b = checks[p] & ~(pos->byColor[them] | attacks[us][PAWN] | attacks[us][KING]);

            if (b) {
                cnt++;
                result -= bb_count(b) * CheckWeight;
            }
        }

    // Bishop X-Ray threats

    bitboard_t bishops = BPseudoAttacks[ks] & pos_pieces_cpp(pos, them, BISHOP, QUEEN);

    while (bishops)
        if (!(Segment[ks][bb_pop_lsb(&bishops)] & pos->byPiece[PAWN])) {
            cnt++;
            result -= CheckWeight;
        }

    // Rook X-Ray threats

    bitboard_t rooks = RPseudoAttacks[ks] & pos_pieces_cpp(pos, them, ROOK, QUEEN);

    while (rooks)
        if (!(Segment[ks][bb_pop_lsb(&rooks)] & pos->byPiece[PAWN])) {
            cnt++;
            result -= CheckWeight;
        }

    return result * (2 + cnt) / 4;
}

static eval_t passer(int us, int pawn, int ourKing, int theirKing)
{
    static const eval_t bonus[] = {{0, 6}, {0, 12}, {22, 30}, {66, 60}, {132, 102}, {220, 156}};

    const int n = relative_rank_of(us, pawn) - RANK_2;

    // score based on rank
    eval_t result = bonus[n];

    // king distance adjustment
    if (n > 1) {
        const int stop = pawn + push_inc(us);
        const int Q = n * (n - 1);
        result.eg += KingDistance[stop][theirKing] * 6 * Q;
        result.eg -= KingDistance[stop][ourKing] * 3 * Q;
    }

    return result;
}

static eval_t do_pawns(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
{
    static const eval_t Isolated[2] = {{20, 40}, {40, 40}};
    static const eval_t Hole[2] = {{16, 20}, {32, 20}};
    static const int shieldBonus[NB_RANK] = {0, 38, 21, 16, 12, 10, 10};

    const int them = opposite(us);
    const bitboard_t ourPawns = pos_pieces_cp(pos, us, PAWN);
    const bitboard_t theirPawns = pos_pieces_cp(pos, them, PAWN);
    const int ourKing = pos_king_square(pos, us);
    const int theirKing = pos_king_square(pos, them);

    eval_t result = {0, 0};

    // Pawn shield

    bitboard_t b = ourPawns & PawnPath[us][ourKing];

    while (b)
        result.op += shieldBonus[relative_rank_of(us, bb_pop_lsb(&b))];

    b = ourPawns & PawnSpan[us][ourKing];

    while (b)
        result.op += shieldBonus[relative_rank_of(us, bb_pop_lsb(&b))] / 2;

    // Pawn structure

    b = ourPawns;

    while (b) {
        const int s = bb_pop_lsb(&b);
        const int stop = s + push_inc(us);
        const int r = rank_of(s), f = file_of(s);

        const bitboard_t adjacentFiles = AdjacentFiles[f];
        const bitboard_t besides = ourPawns & adjacentFiles;

        const bool chained = besides & (bb_rank(r) | bb_rank(us == WHITE ? r - 1 : r + 1));
        const bool phalanx = chained && (ourPawns & PAttacks[them][stop]);
        const bool hole = !(PawnSpan[them][stop] & ourPawns) && bb_test(attacks[them][PAWN], stop);
        const bool isolated = !(adjacentFiles & ourPawns);
        const bool exposed = !(PawnPath[us][s] & pos->byPiece[PAWN]);
        const bool passed = exposed && !(PawnSpan[us][s] & theirPawns);

        if (chained) {
            const int rr = relative_rank(us, r) - RANK_2;
            const int bonus = rr * (rr + phalanx) * 3;
            eval_add(&result, (eval_t) {8 + bonus / 2, bonus});
        } else if (hole)
            eval_sub(&result, Hole[exposed]);
        else if (isolated)
            eval_sub(&result, Isolated[exposed]);

        if (passed)
            eval_add(&result, passer(us, s, ourKing, theirKing));
    }

    return result;
}

static eval_t pawns(const Position *pos, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
// Pawn evaluation is directly a diff, from white's pov. This reduces by half the
// size of the pawn hash table.
{
    const uint64_t key = pos->pawnKey;
    const size_t idx = key & (NB_PAWN_ENTRY - 1);

    if (PawnHash[idx].key == key)
        return PawnHash[idx].eval;

    PawnHash[idx].key = key;
    PawnHash[idx].eval = do_pawns(pos, WHITE, attacks);
    eval_sub(&PawnHash[idx].eval, do_pawns(pos, BLACK, attacks));
    return PawnHash[idx].eval;
}

static int blend(const Position *pos, eval_t e)
{
    static const int full = 4 * (N + B + R) + 2 * Q;
    const int total = pos->pieceMaterial[WHITE].eg+ pos->pieceMaterial[BLACK].eg;
    return e.op * total / full + e.eg * (full - total) / full;
}

void eval_init()
{
    for (int s = H8; s >= A1; --s) {
        if (rank_of(s) == RANK_8)
            PawnSpan[WHITE][s] = PawnPath[WHITE][s] = 0;
        else {
            PawnSpan[WHITE][s] = PAttacks[WHITE][s] | PawnSpan[WHITE][s + UP];
            PawnPath[WHITE][s] = (1ULL << (s + UP)) | PawnPath[WHITE][s + UP];
        }
    }

    for (int s = A1; s <= H8; ++s) {
        if (rank_of(s) == RANK_1)
            PawnSpan[BLACK][s] = PawnPath[BLACK][s] = 0;
        else {
            PawnSpan[BLACK][s] = PAttacks[BLACK][s] | PawnSpan[BLACK][s + DOWN];
            PawnPath[BLACK][s] = (1ULL << (s + DOWN)) | PawnPath[BLACK][s + DOWN];
        }
    }

    for (int f = FILE_A; f <= FILE_H; ++f)
        AdjacentFiles[f] = (f > FILE_A ? bb_file(f - 1) : 0)
                           | (f < FILE_H ? bb_file(f + 1) : 0);

    for (int s1 = A1; s1 <= H8; ++s1)
        for (int s2 = A1; s2 <= H8; ++s2)
            KingDistance[s1][s2] = max(abs(rank_of(s1) - rank_of(s2)), abs(file_of(s1) - file_of(s2)));
}

int evaluate(const Position *pos)
{
    assert(!pos->checkers);
    eval_t e[NB_COLOR] = {pos->pst, {0, 0}};

    bitboard_t attacks[NB_COLOR][NB_PIECE + 1];

    // Mobility first, because it fills in the attacks array
    for (int c = WHITE; c <= BLACK; ++c)
        eval_add(&e[c], mobility(pos, c, attacks));

    for (int c = WHITE; c <= BLACK; ++c) {
        eval_add(&e[c], bishop_pair(pos, c));
        e[c].op += tactics(pos, c, attacks);
        e[c].op += safety(pos, c, attacks);
    }

    eval_add(&e[WHITE], pawns(pos, attacks));

    eval_t stm = e[pos->turn];
    eval_sub(&stm, e[opposite(pos->turn)]);

    return blend(pos, stm);
}
