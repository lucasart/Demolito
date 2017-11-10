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
#include <ctype.h>
#include "bitboard.h"
#include "move.h"
#include "position.h"
#include "pst.h"

static uint64_t ZobristKey[NB_COLOR][NB_PIECE][NB_SQUARE];
static uint64_t ZobristCastling[NB_SQUARE];
static uint64_t ZobristEnPassant[NB_SQUARE + 1];
static uint64_t ZobristTurn;

// SplitMix64 PRNG, by Sebastiano Vigna: http://xoroshiro.di.unimi.it/splitmix64.c
// 64-bit state is enough:
//   * 2^64 period is long enough to generate a few zobrist keys.
//   * Statistically strong: passes TestU01's BigCrunch.
// 64-bit state is, in fact, a huge bonus:
//   * Easy to seed: Any seed is fine, even zero!
//   * Escapes immediately from zero land, unlike large seed generators
// Fast enough: 1.31ns per drawing on i7 7700 CPU @ 3.6 GHz (Kaby Lake).
static uint64_t prng(uint64_t *state)
{
    uint64_t s = (*state += 0x9E3779B97F4A7C15ULL);
    s = (s ^ (s >> 30)) * 0xBF58476D1CE4E5B9ULL;
    s = (s ^ (s >> 27)) * 0x94D049BB133111EBULL;
    s ^= s >> 31;
    assert(s);  // We cannot have a zero key. If it happens, change the seed.
    return s;
}

static uint64_t zobrist_castling(bitboard_t castlableRooks)
{
    assert(bb_count(Rank[RANK_1] & castlableRooks) <= 2);
    assert(bb_count(Rank[RANK_8] & castlableRooks) <= 2);
    assert(!(castlableRooks & ~Rank[RANK_1] & ~Rank[RANK_8]));
    bitboard_t k = 0;

    while (castlableRooks)
        k ^= ZobristCastling[bb_pop_lsb(&castlableRooks)];

    return k;
}

void pos_init()
{
    uint64_t state = 0;

    for (int c = WHITE; c <= BLACK; c++)
        for (int p = KNIGHT; p < NB_PIECE; p++)
            for (int s = A1; s <= H8; s++)
                ZobristKey[c][p][s] = prng(&state);

    for (int s = A1; s <= H8; s++) {
        ZobristCastling[s] = prng(&state);
        ZobristEnPassant[s] = prng(&state);
    }

    ZobristEnPassant[NB_SQUARE] = prng(&state);
    ZobristTurn = prng(&state);
}

static void clear(Position *pos)
{
    memset(pos, 0, sizeof(*pos));
    memset(pos->pieceOn, NB_PIECE, sizeof(pos->pieceOn));
}

static void clear_square(Position *pos, int c, int p, int s)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p, NB_PIECE);
    BOUNDS(s, NB_SQUARE);

    bb_clear(&pos->byColor[c], s);
    bb_clear(&pos->byPiece[p], s);
    pos->pieceOn[s] = NB_PIECE;
    eval_sub(&pos->pst, pst[c][p][s]);
    pos->key ^= ZobristKey[c][p][s];

    if (p <= QUEEN)
        eval_sub(&pos->pieceMaterial[c], Material[p]);
    else
        pos->pawnKey ^= ZobristKey[c][p][s];
}

static void set_square(Position *pos, int c, int p, int s)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p, NB_PIECE);
    BOUNDS(s, NB_SQUARE);

    bb_set(&pos->byColor[c], s);
    bb_set(&pos->byPiece[p], s);
    pos->pieceOn[s] = p;
    eval_add(&pos->pst, pst[c][p][s]);
    pos->key ^= ZobristKey[c][p][s];

    if (p <= QUEEN)
        eval_add(&pos->pieceMaterial[c], Material[p]);
    else
        pos->pawnKey ^= ZobristKey[c][p][s];
}

static bitboard_t attacked_by(const Position *pos, int c)
{
    BOUNDS(c, NB_COLOR);

    // King and Knight attacks
    bitboard_t result = KAttacks[pos_king_square(pos, c)];
    bitboard_t fss = pos_pieces_cp(pos, c, KNIGHT);

    while (fss)
        result |= NAttacks[bb_pop_lsb(&fss)];

    // Pawn captures
    fss = pos_pieces_cp(pos, c, PAWN) & ~File[FILE_A];
    result |= bb_shift(fss, push_inc(c) + LEFT);
    fss = pos_pieces_cp(pos, c, PAWN) & ~File[FILE_H];
    result |= bb_shift(fss, push_inc(c) + RIGHT);

    // Sliders
    bitboard_t _occ = pos_pieces(pos) ^ pos_pieces_cp(pos, opposite(c), KING);
    fss = pos_pieces_cpp(pos, c, ROOK, QUEEN);

    while (fss)
        result |= bb_rattacks(bb_pop_lsb(&fss), _occ);

    fss = pos_pieces_cpp(pos, c, BISHOP, QUEEN);

    while (fss)
        result |= bb_battacks(bb_pop_lsb(&fss), _occ);

    return result;
}

static bitboard_t calc_pins(const Position *pos)
{
    const int us = pos->turn, them = opposite(us);
    const int king = pos_king_square(pos, us);
    bitboard_t pinners = (pos_pieces_cpp(pos, them, ROOK, QUEEN) & RPseudoAttacks[king])
        | (pos_pieces_cpp(pos, them, BISHOP, QUEEN) & BPseudoAttacks[king]);
    bitboard_t result = 0;

    while (pinners) {
        const int s = bb_pop_lsb(&pinners);
        bitboard_t skewered = Segment[king][s] & pos_pieces(pos);
        bb_clear(&skewered, king);
        bb_clear(&skewered, s);

        if (!bb_several(skewered) && (skewered & pos->byColor[us]))
            result |= skewered;
    }

    return result;
}

static void finish(Position *pos)
{
    const int us = pos->turn, them = opposite(us);
    const int ksq = pos_king_square(pos, us);

    pos->attacked = attacked_by(pos, them);
    pos->checkers = bb_test(pos->attacked, ksq)
        ? pos_attackers_to(pos, ksq, pos_pieces(pos)) & pos->byColor[them] : 0;
    pos->pins = calc_pins(pos);
}

void pos_set(Position *pos, const char *fen, bool chess960)
{
    clear(pos);
    char *str = strdup(fen), *strPos = NULL;
    char *token = strtok_r(str, " ", &strPos);

    // int placement
    char c;
    int s = A8;

    while ((c = *token++)) {
        if (isdigit(c))
            s += c - '0';
        else if (c == '/')
            s += 2 * DOWN;
        else {
            for (int col = WHITE; col <= BLACK; ++col) {
                const char *p = strchr(PieceLabel[col], c);

                if (p) {
                    set_square(pos, col, p - PieceLabel[col], s);
                    ++s;
                }
            }
        }
    }

    // Turn of play
    token = strtok_r(NULL, " ", &strPos);

    if (token[0] == 'w')
        pos->turn = WHITE;
    else {
        pos->turn = BLACK;
        pos->key ^= ZobristTurn;
    }

    // Castling rights
    token = strtok_r(NULL, " ", &strPos);

    while ((c = *token++)) {
        const int r = isupper(c) ? RANK_1 : RANK_8;
        c = toupper(c);

        if (c == 'K')
            s = bb_msb(Rank[r] & pos->byPiece[ROOK]);
        else if (c == 'Q')
            s = bb_lsb(Rank[r] & pos->byPiece[ROOK]);
        else if ('A' <= c && c <= 'H')
            s = square(r, c - 'A');
        else
            break;

        bb_set(&pos->castleRooks, s);
    }

    pos->key ^= zobrist_castling(pos->castleRooks);

    // en-passant, 50 move, chess960
    pos->epSquare = string_to_square(strtok_r(NULL, " ", &strPos));
    pos->key ^= ZobristEnPassant[pos->epSquare];
    pos->rule50 = atoi(strtok_r(NULL, " ", &strPos));
    pos->chess960 = chess960;

    free(str);
    finish(pos);
}

void pos_move(Position *pos, const Position *before, move_t m)
{
    assert(move_ok(m));
    *pos = *before;
    pos->rule50++;

    const int us = pos->turn, them = opposite(us);
    const int from = move_from(m), to = move_to(m), prom = move_prom(m);
    const int p = pos->pieceOn[from];
    const int capture = pos->pieceOn[to];

    // Capture piece on to square (if any)
    if (capture != NB_PIECE) {
        pos->rule50 = 0;
        // Use pos_color_on() instead of them, because we could be playing a KxR castling here
        clear_square(pos, pos_color_on(pos, to), capture, to);

        // Capturing a rook alters corresponding castling right
        if (capture == ROOK)
            pos->castleRooks &= ~(1ULL << to);
    }

    // Move our piece
    clear_square(pos, us, p, from);
    set_square(pos, us, p, to);

    if (p == PAWN) {
        // reset rule50, and set epSquare
        const int push = push_inc(us);
        pos->rule50 = 0;
        pos->epSquare = to == from + 2 * push ? from + push : NB_SQUARE;

        // handle ep-capture and promotion
        if (to == before->epSquare)
            clear_square(pos, them, p, to - push);
        else if (rank_of(to) == RANK_8 || rank_of(to) == RANK_1) {
            clear_square(pos, us, p, to);
            set_square(pos, us, prom, to);
        }
    } else {
        pos->epSquare = NB_SQUARE;

        if (p == ROOK)
            // remove corresponding castling right
            pos->castleRooks &= ~(1ULL << from);
        else if (p == KING) {
            // Lose all castling rights
            pos->castleRooks &= ~Rank[us * RANK_8];

            // Castling
            if (bb_test(before->byColor[us], to)) {
                // Capturing our own piece can only be a castling move, encoded KxR
                assert(before->pieceOn[to] == ROOK);
                const int r = rank_of(from);

                clear_square(pos, us, KING, to);
                set_square(pos, us, KING, square(r, to > from ? FILE_G : FILE_C));
                set_square(pos, us, ROOK, square(r, to > from ? FILE_F : FILE_D));
            }
        }
    }

    pos->turn = them;
    pos->key ^= ZobristTurn;
    pos->key ^= ZobristEnPassant[before->epSquare] ^ ZobristEnPassant[pos->epSquare];
    pos->key ^= zobrist_castling(before->castleRooks ^ pos->castleRooks);

    finish(pos);
}

void pos_switch(Position *pos, const Position *before)
{
    *pos = *before;
    pos->epSquare = NB_SQUARE;

    pos->turn = opposite(pos->turn);
    pos->key ^= ZobristTurn;
    pos->key ^= ZobristEnPassant[before->epSquare] ^ ZobristEnPassant[pos->epSquare];

    finish(pos);
}

bitboard_t pos_pieces_cp(const Position *pos, int c, int p)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p, NB_PIECE);
    return pos->byColor[c] & pos->byPiece[p];
}

bitboard_t pos_pieces(const Position *pos)
{
    assert(!(pos->byColor[WHITE] & pos->byColor[BLACK]));
    return pos->byColor[WHITE] | pos->byColor[BLACK];
}

bitboard_t pos_pieces_cpp(const Position *pos, int c, int p1, int p2)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p1, NB_PIECE);
    BOUNDS(p2, NB_PIECE);
    return pos->byColor[c] & (pos->byPiece[p1] | pos->byPiece[p2]);
}

void pos_get(const Position *pos, char *fen)
{
    // int placement
    for (int r = RANK_8; r >= RANK_1; r--) {
        int cnt = 0;

        for (int f = FILE_A; f <= FILE_H; f++) {
            const int s = square(r, f);

            if (bb_test(pos_pieces(pos), s)) {
                if (cnt)
                    *fen++ = cnt + '0';

                *fen++ = PieceLabel[pos_color_on(pos, s)][(int)pos->pieceOn[s]];
                cnt = 0;
            } else
                cnt++;
        }

        if (cnt)
            *fen++ = cnt + '0';

        *fen++ = r == RANK_1 ? ' ' : '/';
    }

    // Turn of play
    *fen++ = pos->turn == WHITE ? 'w' : 'b';
    *fen++ = ' ';

    // Castling rights
    if (!pos->castleRooks)
        *fen++ = '-';
    else {
        for (int c = WHITE; c <= BLACK; c++) {
            const bitboard_t sqs = pos->castleRooks & pos->byColor[c];

            if (!sqs)
                continue;

            const int king = pos_king_square(pos, c);

            // Because we have castlable rooks, the king has to be on the first rank and
            // cannot be in a corner, which allows using Ray[king][king +/- 1] to search
            // for the castle rook in Chess960.
            assert(rank_of(king) == relative_rank(c, RANK_1));
            assert(file_of(king) != FILE_A && file_of(king) != FILE_H);

            // Right side castling
            if (sqs & Ray[king][king + RIGHT]) {
                if (pos->chess960)
                    *fen++ = file_of(bb_lsb(sqs & Ray[king][king + RIGHT])) + (c == WHITE ? 'A' : 'a');
                else
                    *fen++ = PieceLabel[c][KING];
            }

            // Left side castling
            if (sqs & Ray[king][king + LEFT]) {
                if (pos->chess960)
                    *fen++ = file_of(bb_msb(sqs & Ray[king][king + LEFT])) + (c == WHITE ? 'A' : 'a');
                else
                    *fen++ = PieceLabel[c][QUEEN];
            }
        }
    }

    // En passant and 50 move
    char str[5];
    square_to_string(pos->epSquare, str);
    sprintf(fen, " %s %d", str, pos->rule50);
}

bitboard_t pos_ep_square_bb(const Position *pos)
{
    return pos->epSquare < NB_SQUARE ? 1ULL << pos->epSquare : 0;
}

bool pos_insufficient_material(const Position *pos)
{
    return bb_count(pos_pieces(pos)) <= 3 && !pos->byPiece[PAWN] && !pos->byPiece[ROOK]
        && !pos->byPiece[QUEEN];
}

int pos_king_square(const Position *pos, int c)
{
    assert(bb_count(pos_pieces_cp(pos, c, KING)) == 1);
    return bb_lsb(pos_pieces_cp(pos, c, KING));
}

int pos_color_on(const Position *pos, int s)
{
    assert(bb_test(pos_pieces(pos), s));
    return bb_test(pos->byColor[WHITE], s) ? WHITE : BLACK;
}

bitboard_t pos_attackers_to(const Position *pos, int s, bitboard_t occ)
{
    BOUNDS(s, NB_SQUARE);
    return (pos_pieces_cp(pos, WHITE, PAWN) & PAttacks[BLACK][s])
        | (pos_pieces_cp(pos, BLACK, PAWN) & PAttacks[WHITE][s])
        | (NAttacks[s] & pos->byPiece[KNIGHT])
        | (KAttacks[s] & pos->byPiece[KING])
        | (bb_rattacks(s, occ) & (pos->byPiece[ROOK] | pos->byPiece[QUEEN]))
        | (bb_battacks(s, occ) & (pos->byPiece[BISHOP] | pos->byPiece[QUEEN]));
}

void pos_print(const Position *pos)
{
    for (int r = RANK_8; r >= RANK_1; r--) {
        char line[] = ". . . . . . . .";

        for (int f = FILE_A; f <= FILE_H; f++) {
            const int s = square(r, f);
            line[2 * f] = bb_test(pos_pieces(pos), s)
                ? PieceLabel[pos_color_on(pos, s)][(int)pos->pieceOn[s]]
                : s == pos->epSquare ? '*' : '.';
        }

        puts(line);
    }

    char fen[MAX_FEN];
    pos_get(pos, fen);
    puts(fen);

    bitboard_t b = pos->checkers;

    if (b) {
        puts("checkers:");
        char str[5];

        while (b) {
            square_to_string(bb_pop_lsb(&b), str);
            printf(" %s", str);
        }

        puts("");
    }
}
