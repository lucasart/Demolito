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
#include "eval.h"
#include "bitboard.h"
#include "position.h"
#include "tune.h"
#include "uci.h"
#include "util.h"
#include <math.h>
#include <stdlib.h>

// Pre-calculated in eval_init()
static int StartPieceTotal;
static bitboard_t PawnSpan[NB_COLOR][NB_SQUARE];
static bitboard_t PawnPath[NB_COLOR][NB_SQUARE];
static bitboard_t AdjacentFiles[NB_FILE];
static int KingDistance[NB_SQUARE][NB_SQUARE];
static int SafetyCurve[4096];
static double Noise[NB_LEVEL];

static bitboard_t pawn_attacks(const Position *pos, int color) {
    const bitboard_t pawns = pos_pieces_cp(pos, color, PAWN);
    return bb_shift(pawns & ~File[FILE_A], push_inc(color) + LEFT) |
           bb_shift(pawns & ~File[FILE_H], push_inc(color) + RIGHT);
}

static eval_t mobility(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1]) {
    const int them = opposite(us);
    eval_t result = {0, 0};

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
    bitboard_t occ = pos_pieces(pos) ^ rookMovers; // RQ see through each other

    while (rookMovers) {
        const int from = bb_pop_lsb(&rookMovers), piece = pos->pieceOn[from];
        const bitboard_t targets = bb_rook_attacks(from, occ);
        attacks[us][piece] |= targets;
        eval_add(&result, Mobility[2 * piece - ROOK][bb_count(targets & available)]);
    }

    // Diagonal mobility
    bitboard_t bishopMovers = pos_pieces_cpp(pos, us, BISHOP, QUEEN);
    occ = pos_pieces(pos) ^ bishopMovers; // BQ see through each other

    while (bishopMovers) {
        const int from = bb_pop_lsb(&bishopMovers), piece = pos->pieceOn[from];
        const bitboard_t targets = bb_bishop_attacks(from, occ);
        attacks[us][piece] |= targets;
        eval_add(&result, Mobility[piece][bb_count(targets & available)]);
    }

    return result;
}

static eval_t pattern(const Position *pos, int us) {
    const bitboard_t WhiteSquares = 0x55AA55AA55AA55AAULL;
    const bitboard_t ourPawns = pos_pieces_cp(pos, us, PAWN);
    const bitboard_t theirPawns = pos->byPiece[PAWN] ^ ourPawns;
    const bitboard_t ourBishops = pos_pieces_cp(pos, us, BISHOP);

    // Bishop pair
    eval_t result =
        (ourBishops & WhiteSquares) && (ourBishops & ~WhiteSquares) ? BishopPair : (eval_t){0, 0};

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

static eval_t hanging(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1]) {
    const int them = opposite(us);
    eval_t result = {0, 0};

    // Penalize hanging pieces in the opening
    bitboard_t b = attacks[them][PAWN] & (pos->byColor[us] ^ pos_pieces_cp(pos, us, PAWN));
    b |= (attacks[them][KNIGHT] | attacks[them][BISHOP]) & pos_pieces_cpp(pos, us, ROOK, QUEEN);
    b |= pos_pieces_cp(pos, us, QUEEN) & attacks[them][ROOK];
    b |= pos_pieces_cp(pos, us, PAWN) & attacks[them][NB_PIECE] &
         ~(attacks[us][PAWN] | attacks[us][KING] | attacks[us][NB_PIECE]);

    while (b) {
        const int piece = pos->pieceOn[bb_pop_lsb(&b)];
        assert(piece == PAWN || (KNIGHT <= piece && piece <= QUEEN));
        result.op -= Hanging[piece];
    }

    // Penalize hanging pawns in the endgame
    b = pos_pieces_cp(pos, us, PAWN) & attacks[them][KING] &
        ~(attacks[us][PAWN] | attacks[us][KING]);

    if (b)
        result.eg -= Hanging[PAWN] * bb_count(b);

    return result;
}

static int safety(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1]) {
    const int them = opposite(us);
    int weight = 0, count = 0;

    // Attacks around the King
    const bitboard_t dangerZone = attacks[us][KING] & ~attacks[us][PAWN];

    for (int piece = KNIGHT; piece <= PAWN; piece++) {
        const bitboard_t attacked = attacks[them][piece] & dangerZone;

        if (attacked && piece != KING) {
            count++;
            weight += bb_count(attacked) * RingAttack[piece];
            weight -= bb_count(attacked & attacks[us][NB_PIECE]) * RingDefense[piece];
        }
    }

    // Check threats
    const int king = pos_king_square(pos, us);
    const bitboard_t occ = pos_pieces(pos);
    const bitboard_t checks[] = {KnightAttacks[king] & attacks[them][KNIGHT],
                                 bb_bishop_attacks(king, occ) & attacks[them][BISHOP],
                                 bb_rook_attacks(king, occ) & attacks[them][ROOK],
                                 (bb_bishop_attacks(king, occ) | bb_rook_attacks(king, occ)) &
                                     attacks[them][QUEEN]};

    for (int piece = KNIGHT; piece <= QUEEN; piece++)
        if (checks[piece]) {
            const bitboard_t b =
                checks[piece] & ~(pos->byColor[them] | attacks[us][PAWN] | attacks[us][KING]);

            if (b) {
                count++;
                weight += bb_count(b) * CheckAttack[piece];
                weight -= bb_count(b & attacks[us][NB_PIECE]) * CheckDefense[piece];
            }
        }

    // X-Ray threats: sliding pieces with potential for pins or discovered checks
    bitboard_t xrays = (bb_bishop_attacks(king, 0) & pos_pieces_cpp(pos, them, BISHOP, QUEEN)) |
                       (bb_rook_attacks(king, 0) & pos_pieces_cpp(pos, them, ROOK, QUEEN));

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

static eval_t passer(int us, int pawn, int ourKing, int theirKing) {
    const int n = relative_rank_of(us, pawn) - RANK_2;

    // score based on rank
    eval_t result = PasserBonus[n];

    // king distance adjustment
    if (n > 1) {
        const int stop = pawn + push_inc(us);
        result.eg += KingDistance[stop][theirKing] * PasserAdjust[n];
        result.eg -= KingDistance[stop][ourKing] * PasserAdjust[n] / 2;
    }

    return result;
}

static eval_t do_pawns(const Position *pos, int us, bitboard_t attacks[NB_COLOR][NB_PIECE + 1],
                       bitboard_t *passed) {
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
        result.op += Shield[dte][relative_rank_of(us, bb_pop_lsb(&b)) - RANK_2];

    // Pawn structure
    b = ourPawns;

    // Relative king distance bonus
    int maxDistance = 0;

    while (b) {
        const int square = bb_pop_lsb(&b);
        const int stop = square + push_inc(us);
        const int rank = rank_of(square), file = file_of(square);
        const bitboard_t besides = ourPawns & AdjacentFiles[file];
        const bool exposed = !(PawnPath[us][square] & pos->byPiece[PAWN]);

        const int d =
            KingDistance[stop][theirKing] * Distance[1] - KingDistance[stop][ourKing] * Distance[0];
        maxDistance = max(maxDistance, d);

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

    result.eg += maxDistance;

    return result;
}

static eval_t pawns(Worker *worker, const Position *pos, bitboard_t attacks[NB_COLOR][NB_PIECE + 1])
// Pawn evaluation is directly a diff, from white's pov. This halves the size of the table.
{
    const uint64_t key = pos->kingPawnKey;
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
        const int us = pos_color_on(pos, square);
        const int n = relative_rank_of(us, square) - RANK_4;

        if (n >= 0 && !bb_test(occ, square + push_inc(us)))
            e.eg += us == WHITE ? FreePasser[n] : -FreePasser[n];
    }

    return e;
}

static int blend(const Position *pos, eval_t e) {
    const int pieceTotal = pos->pieceMaterial[WHITE] + pos->pieceMaterial[BLACK];
    return (e.op * pieceTotal + e.eg * (StartPieceTotal - pieceTotal)) / StartPieceTotal;
}

void eval_init(void) {
    StartPieceTotal =
        4 * (PieceValue[KNIGHT] + PieceValue[BISHOP] + PieceValue[ROOK]) + 2 * PieceValue[QUEEN];

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
        AdjacentFiles[file] =
            (file > FILE_A ? File[file - 1] : 0) | (file < FILE_H ? File[file + 1] : 0);

    for (int s1 = A1; s1 <= H8; s1++)
        for (int s2 = A1; s2 <= H8; s2++) {
            const int rankDist = abs(rank_of(s1) - rank_of(s2));
            const int fileDist = abs(file_of(s1) - file_of(s2));
            KingDistance[s1][s2] = max(rankDist, fileDist);
        }

    // Validate above calculations in debug mode
    uint64_t h = 0;
    hash_blocks(PawnSpan, sizeof PawnSpan, &h);
    hash_blocks(PawnPath, sizeof PawnPath, &h);
    hash_blocks(AdjacentFiles, sizeof AdjacentFiles, &h);
    hash_blocks(KingDistance, sizeof KingDistance, &h);
    assert(h == 0x9f29705ba9ca230);

    for (int i = 0; i < 4096; i++) {
        const int x = pow((double)i, SafetyCurveParam[0] * 0.001) + 0.5;
        SafetyCurve[i] = x > SafetyCurveParam[1] ? SafetyCurve[i - 1] + 1 : x;
    }

    for (int l = 0; l < NB_LEVEL; l++)
        Noise[l] = pow(0.78, l + (l >= 9));
}

int evaluate(Worker *worker, const Position *pos) {
    worker->nodes++;

    assert(!pos->checkers);
    const int us = pos->turn, them = opposite(us);
    eval_t e[NB_COLOR] = {pos->pst, {0, 0}};

    bitboard_t attacks[NB_COLOR][NB_PIECE + 1] = {{0}, {0}};

    // Calculate attacks[] for king and pawn
    for (int color = WHITE; color <= BLACK; color++) {
        attacks[color][KING] = KingAttacks[pos_king_square(pos, color)];
        attacks[color][PAWN] = pawn_attacks(pos, color);
    }

    // Calculate mobility of pieces (ie. NBRQ), and with it the remaining attacks[]
    for (int color = WHITE; color <= BLACK; color++) {
        eval_add(&e[color], mobility(pos, color, attacks));

        // Aggregate attacks pieces only (NBRQ)
        attacks[color][NB_PIECE] = attacks[color][KNIGHT] | attacks[color][BISHOP] |
                                   attacks[color][ROOK] | attacks[color][QUEEN];
    }

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

    if (pos->pieceMaterial[winner] - pos->pieceMaterial[loser] < PieceValue[ROOK]) {
        // Scale down when the winning side has <= 2 pawns
        static const int discount[9] = {5, 2, 1};
        stm.eg -= stm.eg * discount[bb_count(winnerPawns)] / 8;
    }

    int result = blend(pos, stm);

    if (uciLevel) {
        // Draw 0 < p < 1
        double p = 0;
        while (p <= 0 || p >= 1)
            p = prngf(&worker->seed);

        // Scale down noise towards the endgame
        const double phaseFactor = 0.5 + (double)pos->pieceMaterial[pos->turn] / StartPieceTotal;

        // scale parameter of centered logistic: CDF(x) = 1 / (1 + exp(-x/s))
        const double s = 200 * Noise[uciLevel - 1] * phaseFactor;

        // Add logistic drawing as eval noise
        result += s * log(p / (1 - p));
    }

    return result;
}
