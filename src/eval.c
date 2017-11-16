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
#include "eval.h"
#include "position.h"

// Pre-calculated in eval_init()
static bitboard_t PawnSpan[NB_COLOR][NB_SQUARE];
static bitboard_t PawnPath[NB_COLOR][NB_SQUARE];
static bitboard_t AdjacentFiles[NB_FILE];
static int KingDistance[NB_SQUARE][NB_SQUARE];

static bitboard_t pawn_attacks(const Position *pos, int c)
{
    const bitboard_t pawns = pos_pieces_cp(pos, c, PAWN);
    return bb_shift(pawns & ~File[FILE_A], push_inc(c) + LEFT)
        | bb_shift(pawns & ~File[FILE_H], push_inc(c) + RIGHT);
}

static eval_t score_mobility(int p0, int p, bitboard_t tss)
{
    assert(KNIGHT <= p0 && p0 <= ROOK);
    assert(KNIGHT <= p && p <= QUEEN);

    static /* <-- GCC bug */ const int AdjustCount[][15] = {
        {-4, -2, -1, 0, 1, 2, 3, 4, 4},
        {-5, -3, -2, -1, 0, 1, 2, 3, 4, 5, 5, 6, 6, 7},
        {-6, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 6, 7, 7}
    };
    static /* <-- GCC bug */ const eval_t Weight[] = {{6, 10}, {11, 12}, {6, 6}, {4, 6}};

    const int c = AdjustCount[p0][bb_count(tss)];
    return (eval_t) {Weight[p].op * c, Weight[p].eg * c};
}

static eval_t mobility(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
{
    const int them = opposite(us);
    eval_t result = {0, 0};
    bitboard_t fss, tss, occ;
    int from, piece;

    attacks[us][KING] = KAttacks[pos_king_square(pos, us)];
    attacks[them][PAWN] = pawn_attacks(pos, them);

    for (piece = KNIGHT; piece <= QUEEN; attacks[us][piece++] = 0);

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
    occ = pos_pieces(pos) ^ fss;  // RQ see through each other

    while (fss) {
        tss = bb_rattacks(from = bb_pop_lsb(&fss), occ);
        attacks[us][piece = pos->pieceOn[from]] |= tss;
        eval_add(&result, score_mobility(ROOK, piece, tss & targets));
    }

    // Diagonal mobility
    fss = pos_pieces_cpp(pos, us, BISHOP, QUEEN);
    occ = pos_pieces(pos) ^ fss;  // BQ see through each other

    while (fss) {
        tss = bb_battacks(from = bb_pop_lsb(&fss), occ);
        attacks[us][piece = pos->pieceOn[from]] |= tss;
        eval_add(&result, score_mobility(BISHOP, piece, tss & targets));
    }

    attacks[us][NB_PIECE] = attacks[us][KNIGHT] | attacks[us][BISHOP] | attacks[us][ROOK]
        | attacks[us][QUEEN];

    return result;
}

static eval_t pattern(const Position *pos, int us)
{
    const int RookOpen[] = {20, 30};  // 0: semi-open, 1: fully-open
    const eval_t BishopPair[] = {{0, 0}, {83, 110}};  // 0: false, 1: true
    const int Ahead = 16;

    const bitboard_t WhiteSquares = 0x55AA55AA55AA55AAULL;
    const bitboard_t ourPawns = pos_pieces_cp(pos, us, PAWN);
    const bitboard_t theirPawns = pos->byPiece[PAWN] ^ ourPawns;
    const bitboard_t ourBishops = pos_pieces_cp(pos, us, BISHOP);

    // Bishop pair
    eval_t result = BishopPair[(ourBishops & WhiteSquares) && (ourBishops & ~WhiteSquares)];

    // Rook on open file
    bitboard_t b = pos_pieces_cp(pos, us, ROOK);

    while (b) {
        const bitboard_t ahead = PawnPath[us][bb_pop_lsb(&b)];

        if (!(ourPawns & ahead))
            result.op += RookOpen[!(theirPawns & ahead)];
    }

    // Penalize pieces ahead of pawns
    const bitboard_t pawnsBehind = bb_shift(ourPawns, push_inc(us)) & (pos->byColor[us] ^ ourPawns);

    if (pawnsBehind)
        result.op -= Ahead * bb_count(pawnsBehind);

    return result;
}

static int hanging(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
{
    const int Hanging[] = {98, 64, 102, 181};

    const int them = opposite(us);
    int result = 0, cnt = 0;

    // Penalize hanging pieces
    bitboard_t b = attacks[them][PAWN] & (pos->byColor[us] ^ pos_pieces_cp(pos, us, PAWN));
    b |= (attacks[them][KNIGHT] | attacks[them][BISHOP]) & pos_pieces_cpp(pos, us, ROOK, QUEEN);
    b |= attacks[them][ROOK] & pos_pieces_cp(pos, us, QUEEN);

    while (b) {
        const int p = pos->pieceOn[bb_pop_lsb(&b)];
        assert(KNIGHT <= p && p <= QUEEN);
        result -= Hanging[p];
        cnt++;
    }

    if (cnt >= 2)  // Quadratic with number of hanging pieces
        result *= cnt;

    return result;
}

static int safety(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
{
    const int RingAttack[] = {31, 38, 67, 60};
    const int RingDefense[] = {18, 18, 31, 32};
    const int CheckAttack[] = {61, 76, 74, 81};
    const int CheckDefense[] = {26, 34, 30, 34};
    const int BishopXRay = 56, RookXRay = 83;

    const int them = opposite(us);
    int result = 0, cnt = 0;

    // Attacks around the King
    const bitboard_t dangerZone = attacks[us][KING] & ~attacks[us][PAWN];

    for (int p = KNIGHT; p <= QUEEN; p++) {
        const bitboard_t attacked = attacks[them][p] & dangerZone;

        if (attacked) {
            cnt++;
            result -= bb_count(attacked) * RingAttack[p];
            result += bb_count(attacked & attacks[us][NB_PIECE]) * RingDefense[p];
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

    for (int p = KNIGHT; p <= QUEEN; p++)
        if (checks[p]) {
            const bitboard_t b = checks[p] & ~(pos->byColor[them] | attacks[us][PAWN]
                | attacks[us][KING]);

            if (b) {
                cnt++;
                result -= bb_count(b) * CheckAttack[p];
                result += bb_count(b & attacks[us][NB_PIECE]) * CheckDefense[p];
            }
        }

    // Bishop X-Ray threats
    bitboard_t bishops = BPseudoAttacks[ks] & pos_pieces_cpp(pos, them, BISHOP, QUEEN);

    while (bishops)
        if (!(Segment[ks][bb_pop_lsb(&bishops)] & pos->byPiece[PAWN])) {
            cnt++;
            result -= BishopXRay;
        }

    // Rook X-Ray threats
    bitboard_t rooks = RPseudoAttacks[ks] & pos_pieces_cpp(pos, them, ROOK, QUEEN);

    while (rooks)
        if (!(Segment[ks][bb_pop_lsb(&rooks)] & pos->byPiece[PAWN])) {
            cnt++;
            result -= RookXRay;
        }

    return result * (2 + cnt) / 4;
}

static eval_t passer(int us, int pawn, int ourKing, int theirKing)
{
    const eval_t bonus[] = {{0, 6}, {0, 14}, {23, 28}, {51, 69}, {144, 149}, {285, 264}};
    const int adjust[] = {0, 0, 10, 41, 82, 112};

    const int n = relative_rank_of(us, pawn) - RANK_2;

    // score based on rank
    eval_t result = bonus[n];

    // king distance adjustment
    if (n > 1) {
        const int stop = pawn + push_inc(us);
        result.eg += KingDistance[stop][theirKing] * adjust[n];
        result.eg -= KingDistance[stop][ourKing] * adjust[n] / 2;
    }

    return result;
}

static eval_t do_pawns(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1],
    bitboard_t *passed)
{
    const eval_t Isolated[2] = {{19, 33}, {41, 34}};
    const eval_t Backward[2] = {{17, 18}, {29, 22}};
    const eval_t Doubled = {15, 30};
    const int Shield[NB_RANK] = {0, 23, 17, 12, 10, 8, 8};

    const int them = opposite(us);
    const bitboard_t ourPawns = pos_pieces_cp(pos, us, PAWN);
    const bitboard_t theirPawns = pos_pieces_cp(pos, them, PAWN);
    const int ourKing = pos_king_square(pos, us);
    const int theirKing = pos_king_square(pos, them);

    eval_t result = {0, 0};

    // Pawn shield
    bitboard_t b = ourPawns & (PawnPath[us][ourKing] | PawnSpan[us][ourKing]);

    while (b)
        result.op += Shield[relative_rank_of(us, bb_pop_lsb(&b))];

    // Pawn structure
    b = ourPawns;

    while (b) {
        const int s = bb_pop_lsb(&b);
        const int stop = s + push_inc(us);
        const int r = rank_of(s), f = file_of(s);
        const bitboard_t besides = ourPawns & AdjacentFiles[f];
        const bool exposed = !(PawnPath[us][s] & pos->byPiece[PAWN]);

        if (besides & (Rank[r] | Rank[us == WHITE ? r - 1 : r + 1])) {
            const eval_t Connected[] = {{8, 0}, {9, 3}, {14, 11}, {22, 27}, {33, 50}, {45, 74}};
            eval_add(&result, Connected[relative_rank(us, r) - RANK_2]);
        } else if (!(PawnSpan[them][stop] & ourPawns) && bb_test(attacks[them][PAWN], stop))
            eval_sub(&result, Backward[exposed]);
        else if (!besides)
            eval_sub(&result, Isolated[exposed]);

        if (bb_test(ourPawns, stop))
            eval_sub(&result, Doubled);

        if (exposed && !(PawnSpan[us][s] & theirPawns)) {
            bb_set(passed, s);
            eval_add(&result, passer(us, s, ourKing, theirKing));
        }
    }

    return result;
}

static eval_t pawns(Worker *worker, const Position *pos, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
// Pawn evaluation is directly a diff, from white's pov. This halves the size of the table.
{
    const int FreePasser[] = {10, 20, 40, 80};

    const uint64_t key = pos->pawnKey;
    PawnEntry *pe = &worker->pawnHash[key & (NB_PAWN_ENTRY - 1)];
    eval_t e;

    // First the king+pawn squeleton only, using PawhHash
    if (pe->key == key)
        e = pe->eval;
    else {
        pe->key = key;
        pe->passed = 0;
        pe->eval = do_pawns(pos, WHITE, attacks, &pe->passed);
        eval_sub(&pe->eval, do_pawns(pos, BLACK, attacks, &pe->passed));
        e = pe->eval;
    }

    // Second, interaction between pawns and pieces
    const bitboard_t occ = pos_pieces(pos);
    bitboard_t b = pe->passed;

    while (b) {
        const int s = bb_pop_lsb(&b);
        const int us = pos_color_on(pos, s), them = opposite(us);
        const bitboard_t potentialThreats = File[file_of(s)] & pos_pieces_cpp(pos, them, ROOK, QUEEN);
        const int n = relative_rank_of(us, s) - RANK_4;

        if (n >= 0 && !bb_test(occ, s + push_inc(us))
                && (!potentialThreats || !(bb_rattacks(s, occ) & potentialThreats)))
            e.eg += FreePasser[n] * (us == WHITE ? 1 : -1);
    }

    return e;
}

static int blend(const Position *pos, eval_t e)
{
    const int full = 4 * (N + B + R) + 2 * Q;
    const int total = pos->pieceMaterial[WHITE].eg + pos->pieceMaterial[BLACK].eg;

    return e.op * total / full + e.eg * (full - total) / full;
}

void eval_init()
{
    for (int s = H8; s >= A1; s--) {
        if (rank_of(s) == RANK_8)
            PawnSpan[WHITE][s] = PawnPath[WHITE][s] = 0;
        else {
            PawnSpan[WHITE][s] = PAttacks[WHITE][s] | PawnSpan[WHITE][s + UP];
            PawnPath[WHITE][s] = (1ULL << (s + UP)) | PawnPath[WHITE][s + UP];
        }
    }

    for (int s = A1; s <= H8; s++) {
        if (rank_of(s) == RANK_1)
            PawnSpan[BLACK][s] = PawnPath[BLACK][s] = 0;
        else {
            PawnSpan[BLACK][s] = PAttacks[BLACK][s] | PawnSpan[BLACK][s + DOWN];
            PawnPath[BLACK][s] = (1ULL << (s + DOWN)) | PawnPath[BLACK][s + DOWN];
        }
    }

    for (int f = FILE_A; f <= FILE_H; f++)
        AdjacentFiles[f] = (f > FILE_A ? File[f - 1] : 0) | (f < FILE_H ? File[f + 1] : 0);

    for (int s1 = A1; s1 <= H8; ++s1)
        for (int s2 = A1; s2 <= H8; ++s2) {
            const int rankDist = abs(rank_of(s1) - rank_of(s2));
            const int fileDist = abs(file_of(s1) - file_of(s2));
            KingDistance[s1][s2] = max(rankDist, fileDist);
        }
}

int evaluate(Worker *worker, const Position *pos)
{
    assert(!pos->checkers);
    const int us = pos->turn, them = opposite(us);
    eval_t e[NB_COLOR] = {pos->pst, {0, 0}};

    bitboard_t attacks[NB_COLOR][NB_PIECE + 1];

    // Mobility first, because it fills in the attacks array
    for (int c = WHITE; c <= BLACK; c++)
        eval_add(&e[c], mobility(pos, c, attacks));

    for (int c = WHITE; c <= BLACK; c++) {
        e[c].op += hanging(pos, c, attacks);
        e[c].op += safety(pos, c, attacks);
        eval_add(&e[c], pattern(pos, c));
    }

    eval_add(&e[WHITE], pawns(worker, pos, attacks));

    eval_t stm = e[us];
    eval_sub(&stm, e[them]);

    // Scaling rule for endgame
    const int winner = stm.eg > 0 ? us : them, loser = opposite(winner);
    const bitboard_t winnerPawns = pos_pieces_cp(pos, winner, PAWN);

    if (!bb_several(winnerPawns)
            && pos->pieceMaterial[winner].eg - pos->pieceMaterial[loser].eg < R) {
        if (!winnerPawns)
            stm.eg /= 2;
        else {
            assert(bb_count(winnerPawns) == 1);
            stm.eg -= stm.eg / 4;
        }
    }

    return blend(pos, stm);
}
