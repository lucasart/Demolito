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
#include <math.h>
#include <stdlib.h>
#include "bitboard.h"
#include "eval.h"
#include "position.h"

// Pre-calculated in eval_init()
static bitboard_t PawnSpan[NB_COLOR][NB_SQUARE];
static bitboard_t PawnPath[NB_COLOR][NB_SQUARE];
static bitboard_t AdjacentFiles[NB_FILE];
static int KingDistance[NB_SQUARE][NB_SQUARE];
static int SafetyCurve[4096];

static bitboard_t pawn_attacks(const Position *pos, int color)
{
    const bitboard_t pawns = pos_pieces_cp(pos, color, PAWN);
    return bb_shift(pawns & ~File[FILE_A], push_inc(color) + LEFT)
        | bb_shift(pawns & ~File[FILE_H], push_inc(color) + RIGHT);
}

static eval_t mobility(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
{
    static const eval_t Mobility[5][15] = {
        // Knight
        {{-38, -48}, {-18, -30}, {-15, -21}, {4, 0}, {9, 16}, {20, 34}, {34, 28}, {35, 46},
            {29, 47}},
        // Bishop
        {{-66, -70}, {-42, -35}, {-16, -30}, {-7, -19}, {12, -2}, {24, 16}, {37, 33}, {27, 50},
            {41, 55}, {45, 65}, {55, 37}, {71, 65}, {71, 56}, {120, 95}},
        // Rook
        {{-60, -46}, {-33, -51}, {-19, -28}, {-6, -22}, {-5, -3}, {-5, 1}, {9, 2}, {19, 5},
            {27, 19}, {41, 38}, {57, 33}, {56, 40}, {48, 43}, {57, 48}, {72, 56}},
        // Queen diagonal
        {{-27, -41}, {-9, -29}, {-12, -4}, {-4, -8}, {0, -1}, {11, 13}, {11, 23}, {17, 41},
            {13, 13}, {23, 47}, {28, 50}, {34, 24}, {16, 32}, {24, 87}},
        // Queen orthogonal
        {{-24, -64}, {-19, -25}, {-16, -11}, {-9, -4}, {-2, -10}, {4, -10}, {2, 16}, {9, 14},
            {13, 14}, {21, 26}, {8, 37}, {21, 43}, {25, 48}, {19, 45}, {24, 54}}
    };

    const int them = opposite(us);
    eval_t result = {0, 0};
    bitboard_t occ;
    int from, piece;

    attacks[us][KING] = KingAttacks[pos_king_square(pos, us)];
    attacks[them][PAWN] = pawn_attacks(pos, them);

    for (piece = KNIGHT; piece <= QUEEN; attacks[us][piece++] = 0);

    const bitboard_t available = ~(pos_pieces_cpp(pos, us, KING, PAWN) | attacks[them][PAWN]);

    // Knight mobility
    bitboard_t knights = pos_pieces_cp(pos, us, KNIGHT);

    while (knights) {
        bitboard_t targets = KnightAttacks[bb_pop_lsb(&knights)];
        attacks[us][KNIGHT] |= targets;
        eval_add(&result, Mobility[KNIGHT][bb_count(targets & available)]);
    }

    // Lateral mobility
    bitboard_t rookMovers = pos_pieces_cpp(pos, us, ROOK, QUEEN);
    occ = pos_pieces(pos) ^ rookMovers;  // RQ see through each other

    while (rookMovers) {
        bitboard_t targets = bb_rook_attacks(from = bb_pop_lsb(&rookMovers), occ);
        attacks[us][piece = pos_piece_on(pos, from)] |= targets;
        eval_add(&result, Mobility[2 * piece - ROOK][bb_count(targets & available)]);
    }

    // Diagonal mobility
    bitboard_t bishopMovers = pos_pieces_cpp(pos, us, BISHOP, QUEEN);
    occ = pos_pieces(pos) ^ bishopMovers;  // BQ see through each other

    while (bishopMovers) {
        bitboard_t targets = bb_bishop_attacks(from = bb_pop_lsb(&bishopMovers), occ);
        attacks[us][piece = pos_piece_on(pos, from)] |= targets;
        eval_add(&result, Mobility[piece][bb_count(targets & available)]);
    }

    attacks[us][NB_PIECE] = attacks[us][KNIGHT] | attacks[us][BISHOP] | attacks[us][ROOK]
        | attacks[us][QUEEN];

    return result;
}

static eval_t pattern(const Position *pos, int us)
{
    static const int RookOpen[] = {20, 33};  // 0: semi-open, 1: fully-open
    static const eval_t BishopPair[] = {{0, 0}, {77, 122}};  // 0: false, 1: true
    static const int Ahead = 20;

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

static eval_t hanging(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
{
    static const int Hanging[] = {119, 71, 118, 233, 0, 42};

    const int them = opposite(us);
    eval_t result = {0, 0};

    // Penalize hanging pieces in the opening
    bitboard_t b = attacks[them][PAWN] & (pos->byColor[us] ^ pos_pieces_cp(pos, us, PAWN));
    b |= (attacks[them][KNIGHT] | attacks[them][BISHOP]) & pos_pieces_cpp(pos, us, ROOK, QUEEN);
    b |= pos_pieces_cp(pos, us, QUEEN) & attacks[them][ROOK];
    b |= pos_pieces_cp(pos, us, PAWN) & attacks[them][NB_PIECE]
        & ~(attacks[us][PAWN] | attacks[us][KING] | attacks[us][NB_PIECE]);

    while (b) {
        const int piece = pos_piece_on(pos, bb_pop_lsb(&b));
        assert(piece == PAWN || (KNIGHT <= piece && piece <= QUEEN));
        result.op -= Hanging[piece];
    }

    // Penalize hanging pawns in the endgame
    b = pos_pieces_cp(pos, us, PAWN) & attacks[them][KING]
        & ~(attacks[us][PAWN] | attacks[us][KING]);

    if (b)
        result.eg -= Hanging[PAWN] * bb_count(b);

    return result;
}

static int safety(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
{
    static const int RingAttack[] = {37, 51, 72, 68};
    static const int RingDefense[] = {24, 29, 49, 48};
    static const int CheckAttack[] = {77, 101, 98, 95};
    static const int CheckDefense[] = {34, 52, 41, 48};
    static const int XRay[] = {0, 79, 111, 93};

    const int them = opposite(us);
    int weight = 0, count = 0;

    // Attacks around the King
    const bitboard_t dangerZone = attacks[us][KING] & ~attacks[us][PAWN];

    for (int piece = KNIGHT; piece <= QUEEN; piece++) {
        const bitboard_t attacked = attacks[them][piece] & dangerZone;

        if (attacked) {
            count++;
            weight += bb_count(attacked) * RingAttack[piece];
            weight -= bb_count(attacked & attacks[us][NB_PIECE]) * RingDefense[piece];
        }
    }

    // Check threats
    const int king = pos_king_square(pos, us);
    const bitboard_t occ = pos_pieces(pos);
    const bitboard_t checks[] = {
        KnightAttacks[king] & attacks[them][KNIGHT],
        bb_bishop_attacks(king, occ) & attacks[them][BISHOP],
        bb_rook_attacks(king, occ) & attacks[them][ROOK],
        (bb_bishop_attacks(king, occ) | bb_rook_attacks(king, occ)) & attacks[them][QUEEN]
    };

    for (int piece = KNIGHT; piece <= QUEEN; piece++)
        if (checks[piece]) {
            const bitboard_t b = checks[piece] & ~(pos->byColor[them] | attacks[us][PAWN]
                | attacks[us][KING]);

            if (b) {
                count++;
                weight += bb_count(b) * CheckAttack[piece];
                weight -= bb_count(b & attacks[us][NB_PIECE]) * CheckDefense[piece];
            }
        }

    // X-Ray threats: sliding pieces with potential for pins or discovered checks
    bitboard_t xrays = (BishopPseudoAttacks[king] & pos_pieces_cpp(pos, them, BISHOP, QUEEN))
        | (RookPseudoAttacks[king] & pos_pieces_cpp(pos, them, ROOK, QUEEN));

    while (xrays) {
        const int xray = bb_pop_lsb(&xrays);

        if (!(Segment[king][xray] & pos->byPiece[PAWN])) {
            count++;
            weight += XRay[pos->pieceOn[xray]];
        }
    }

    const int idx = weight * (1 + count) / 4;
    return -SafetyCurve[min(idx, 4095)];
}

static eval_t passer(int us, int pawn, int ourKing, int theirKing)
{
    static const eval_t bonus[] = {{0, 5}, {0, 15}, {24, 25}, {53, 63}, {147, 159}, {273, 275}};
    static const int adjust[] = {0, 0, 12, 49, 78, 99};

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
    static const eval_t Isolated[2] = {{15, 25}, {41, 25}};
    static const eval_t Backward[2] = {{12, 18}, {37, 15}};
    static const eval_t Doubled = {28, 36};
    static const int Shield[4][NB_RANK] = {
      {0, 21, 19, 8, 12, 17, 11, 0},
      {0, 31, 23, 5, 7, 5, 3, 0},
      {0, 25, 17, 12, 14, 17, 13, 0},
      {0, 25, 18, 16, 10, 7, 6, 0}
    };
    static const eval_t Connected[] = {{6, -5}, {13, 5}, {18, 6}, {38, 22}, {32, 61}, {51, 63}};

    const int them = opposite(us);
    const bitboard_t ourPawns = pos_pieces_cp(pos, us, PAWN);
    const bitboard_t theirPawns = pos_pieces_cp(pos, them, PAWN);
    const int ourKing = pos_king_square(pos, us);
    const int theirKing = pos_king_square(pos, them);

    eval_t result = {0, 0};

    // Pawn shield
    const int kf = file_of(ourKing), dte = kf > FILE_D ? FILE_H - kf : kf;
    bitboard_t b = ourPawns & (PawnPath[us][ourKing] | PawnSpan[us][ourKing]);

    while (b)
        result.op += Shield[dte][relative_rank_of(us, bb_pop_lsb(&b))];

    // Pawn structure
    b = ourPawns;

    while (b) {
        const int square = bb_pop_lsb(&b);
        const int stop = square + push_inc(us);
        const int rank = rank_of(square), file = file_of(square);
        const bitboard_t besides = ourPawns & AdjacentFiles[file];
        const bool exposed = !(PawnPath[us][square] & pos->byPiece[PAWN]);

        if (besides & (Rank[rank] | Rank[us == WHITE ? rank - 1 : rank + 1]))
            eval_add(&result, Connected[relative_rank(us, rank) - RANK_2]);
        else if (!(PawnSpan[them][stop] & ourPawns) && bb_test(attacks[them][PAWN], stop))
            eval_sub(&result, Backward[exposed]);
        else if (!besides)
            eval_sub(&result, Isolated[exposed]);

        if (bb_test(ourPawns, stop))
            eval_sub(&result, Doubled);

        if (exposed && !(PawnSpan[us][square] & theirPawns)) {
            bb_set(passed, square);
            eval_add(&result, passer(us, square, ourKing, theirKing));
        }

        // In the endgame, keep the enemy king away from our pawns, and ours closer
        result.eg += KingDistance[square][theirKing] - KingDistance[square][ourKing];
    }

    return result;
}

static eval_t pawns(Worker *worker, const Position *pos, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
// Pawn evaluation is directly a diff, from white's pov. This halves the size of the table.
{
    static const int FreePasser[] = {16, 16, 43, 98};

    const uint64_t key = pos->pawnKey;
    PawnEntry *pe = &worker->pawnHash[key % NB_PAWN_HASH];
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
        const int square = bb_pop_lsb(&b);
        const int us = pos_color_on(pos, square), them = opposite(us);
        const bitboard_t potentialThreats = File[file_of(square)]
            & pos_pieces_cpp(pos, them, ROOK, QUEEN);
        const int n = relative_rank_of(us, square) - RANK_4;

        if (n >= 0 && !bb_test(occ, square + push_inc(us))
                && (!potentialThreats || !(bb_rook_attacks(square, occ) & potentialThreats)))
            e.eg += FreePasser[n] * (us == WHITE ? 1 : -1);
    }

    return e;
}

static int blend(const Position *pos, eval_t e)
{
    static const int full = 4 * (N + B + R) + 2 * Q;

    const int total = pos->pieceMaterial[WHITE].eg + pos->pieceMaterial[BLACK].eg;
    return e.op * total / full + e.eg * (full - total) / full;
}

void eval_init()
{
    for (int square = H8; square >= A1; square--) {
        if (rank_of(square) == RANK_8)
            PawnSpan[WHITE][square] = PawnPath[WHITE][square] = 0;
        else {
            PawnSpan[WHITE][square] = PawnAttacks[WHITE][square] | PawnSpan[WHITE][square + UP];
            PawnPath[WHITE][square] = (1ULL << (square + UP)) | PawnPath[WHITE][square + UP];
        }
    }

    for (int square = A1; square <= H8; square++) {
        if (rank_of(square) == RANK_1)
            PawnSpan[BLACK][square] = PawnPath[BLACK][square] = 0;
        else {
            PawnSpan[BLACK][square] = PawnAttacks[BLACK][square] | PawnSpan[BLACK][square + DOWN];
            PawnPath[BLACK][square] = (1ULL << (square + DOWN)) | PawnPath[BLACK][square + DOWN];
        }
    }

    for (int file = FILE_A; file <= FILE_H; file++)
        AdjacentFiles[file] = (file > FILE_A ? File[file - 1] : 0) | (file < FILE_H ? File[file + 1] : 0);

    for (int s1 = A1; s1 <= H8; s1++)
        for (int s2 = A1; s2 <= H8; s2++) {
            const int rankDist = abs(rank_of(s1) - rank_of(s2));
            const int fileDist = abs(file_of(s1) - file_of(s2));
            KingDistance[s1][s2] = max(rankDist, fileDist);
        }

    for (int i = 0; i < 4096; i++) {
        const int x = pow((double)i, 1.062) + 0.5;
        SafetyCurve[i] = x > 800 ? SafetyCurve[i - 1] + 1 : x;
    }
}

int evaluate(Worker *worker, const Position *pos)
{
    assert(!pos->checkers);
    const int us = pos->turn, them = opposite(us);
    eval_t e[NB_COLOR] = {pos->pst, {0, 0}};

    bitboard_t attacks[NB_COLOR][NB_PIECE + 1];

    // Mobility first, because it fills in the attacks array
    for (int color = WHITE; color <= BLACK; color++)
        eval_add(&e[color], mobility(pos, color, attacks));

    for (int color = WHITE; color <= BLACK; color++) {
        e[color].op += safety(pos, color, attacks);
        eval_add(&e[color], hanging(pos, color, attacks));
        eval_add(&e[color], pattern(pos, color));
    }

    eval_add(&e[WHITE], pawns(worker, pos, attacks));

    eval_t stm = e[us];
    eval_sub(&stm, e[them]);

    // Scaling rule for endgame
    assert(!pos_insufficient_material(pos));
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
