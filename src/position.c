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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitboard.h"
#include "move.h"
#include "position.h"

static uint64_t ZobristKey[NB_COLOR][NB_PIECE][NB_SQUARE];
static uint64_t ZobristCastling[NB_SQUARE];
static uint64_t ZobristEnPassant[NB_SQUARE + 1];
static uint64_t ZobristTurn;

// SplitMix64 PRNG, by Sebastiano Vigna: http://xoroshiro.di.unimi.it/splitmix64.c
static uint64_t prng(uint64_t *state)
{
    uint64_t s = (*state += 0x9E3779B97F4A7C15ULL);
    s = (s ^ (s >> 30)) * 0xBF58476D1CE4E5B9ULL;
    s = (s ^ (s >> 27)) * 0x94D049BB133111EBULL;
    s ^= s >> 31;
    assert(s);  // We cannot have a zero key for zobrist hashing. If it happens, change the seed.
    return s;
}

// Combined zobrist mask of all castlable rooks
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

// Sets the position in its empty state (no pieces, white to play, rule50=0, etc.)
static void clear(Position *pos)
{
    memset(pos, 0, sizeof(*pos));
    memset(pos->pieceOn, NB_PIECE, sizeof(pos->pieceOn));
}

// Remove piece 'p' of color 'c' on square 's'. Such a piece must be there first.
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

// Put piece 'p' of color 'c' on square 's'. Square must be empty first.
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

// Squares attacked by pieces of color 'c'
static bitboard_t attacked_by(const Position *pos, int c)
{
    BOUNDS(c, NB_COLOR);

    // King and Knight attacks
    bitboard_t result = KingAttacks[pos_king_square(pos, c)];
    bitboard_t fss = pos_pieces_cp(pos, c, KNIGHT);

    while (fss)
        result |= KnightAttacks[bb_pop_lsb(&fss)];

    // Pawn captures
    fss = pos_pieces_cp(pos, c, PAWN) & ~File[FILE_A];
    result |= bb_shift(fss, push_inc(c) + LEFT);
    fss = pos_pieces_cp(pos, c, PAWN) & ~File[FILE_H];
    result |= bb_shift(fss, push_inc(c) + RIGHT);

    // Sliders
    bitboard_t _occ = pos_pieces(pos) ^ pos_pieces_cp(pos, opposite(c), KING);
    fss = pos_pieces_cpp(pos, c, ROOK, QUEEN);

    while (fss)
        result |= bb_rook_attacks(bb_pop_lsb(&fss), _occ);

    fss = pos_pieces_cpp(pos, c, BISHOP, QUEEN);

    while (fss)
        result |= bb_bishop_attacks(bb_pop_lsb(&fss), _occ);

    return result;
}

// Pinned pieces for the side to move
static bitboard_t calc_pins(const Position *pos)
{
    const int us = pos->turn, them = opposite(us);
    const int king = pos_king_square(pos, us);
    bitboard_t pinners = (pos_pieces_cpp(pos, them, ROOK, QUEEN) & RookPseudoAttacks[king])
        | (pos_pieces_cpp(pos, them, BISHOP, QUEEN) & BishopPseudoAttacks[king]);
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

// Helper function used to facorize common tasks, after setting up a position
static void finish(Position *pos)
{
    const int us = pos->turn, them = opposite(us);
    const int king = pos_king_square(pos, us);

    pos->attacked = attacked_by(pos, them);
    pos->checkers = bb_test(pos->attacked, king)
        ? pos_attackers_to(pos, king, pos_pieces(pos)) & pos->byColor[them] : 0;
    pos->pins = calc_pins(pos);
}

const char *PieceLabel[NB_COLOR] = {"NBRQKP.", "nbrqkp."};

void square_to_string(int s, char *str)
{
    BOUNDS(s, NB_SQUARE + 1);

    if (s == NB_SQUARE)
        *str++ = '-';
    else {
        *str++ = file_of(s) + 'a';
        *str++ = rank_of(s) + '1';
    }

    *str = '\0';
}

int string_to_square(const char *str)
{
    return *str != '-'
           ? square(str[1] - '1', str[0] - 'a')
           : NB_SQUARE;
}

// Initialize pre-calculated data used in this module
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

// Set position from FEN string
void pos_set(Position *pos, const char *fen)
{
    clear(pos);
    char *str = strdup(fen), *strPos = NULL;
    char *token = strtok_r(str, " ", &strPos);

    // Piece placement
    char ch;
    int s = A8;

    while ((ch = *token++)) {
        if (isdigit(ch))
            s += ch - '0';
        else if (ch == '/')
            s += 2 * DOWN;
        else {
            const bool c = islower(ch);
            const char *p = strchr(PieceLabel[c], ch);

            if (p)
                set_square(pos, c, p - PieceLabel[c], s++);
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

    while ((ch = *token++)) {
        const int r = isupper(ch) ? RANK_1 : RANK_8;
        ch = toupper(ch);

        if (ch == 'K')
            s = bb_msb(Rank[r] & pos->byPiece[ROOK]);
        else if (ch == 'Q')
            s = bb_lsb(Rank[r] & pos->byPiece[ROOK]);
        else if ('A' <= ch && ch <= 'H')
            s = square(r, ch - 'A');
        else
            break;

        bb_set(&pos->castleRooks, s);
    }

    pos->key ^= zobrist_castling(pos->castleRooks);

    // en-passant, 50 move
    pos->epSquare = string_to_square(strtok_r(NULL, " ", &strPos));
    pos->key ^= ZobristEnPassant[pos->epSquare];
    pos->rule50 = atoi(strtok_r(NULL, " ", &strPos));

    free(str);
    finish(pos);
}

// Get FEN string of position
void pos_get(const Position *pos, char *fen)
{
    // Piece placement
    for (int r = RANK_8; r >= RANK_1; r--) {
        int cnt = 0;

        for (int f = FILE_A; f <= FILE_H; f++) {
            const int s = square(r, f);

            if (bb_test(pos_pieces(pos), s)) {
                if (cnt)
                    *fen++ = cnt + '0';

                *fen++ = PieceLabel[pos_color_on(pos, s)][pos_piece_on(pos, s)];
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
            if (sqs & Ray[king][king + RIGHT])
                *fen++ = PieceLabel[c][KING];

            // Left side castling
            if (sqs & Ray[king][king + LEFT])
                *fen++ = PieceLabel[c][QUEEN];
        }
    }

    // En passant and 50 move
    char str[3];
    square_to_string(pos->epSquare, str);
    sprintf(fen, " %s %d", str, pos->rule50);
}

// Play a move on a position copy (original 'before' is untouched): pos = before + play(m)
void pos_move(Position *pos, const Position *before, move_t m)
{
    assert(move_ok(m));
    *pos = *before;

    pos->rule50++;
    pos->epSquare = NB_SQUARE;

    const int us = pos->turn, them = opposite(us);
    const int from = move_from(m), to = move_to(m), prom = move_prom(m);
    const int p = pos_piece_on(pos, from);
    const int capture = pos_piece_on(pos, to);

    // Capture piece on to square (if any)
    if (capture != NB_PIECE) {
        pos->rule50 = 0;
        // Use pos_color_on() instead of them, because we could be playing a KxR castling here
        clear_square(pos, pos_color_on(pos, to), capture, to);

        // Capturing a rook alters corresponding castling right
        pos->castleRooks &= ~(1ULL << to);
    }

    if (p <= QUEEN) {
        // Move piece (branch-less)
        clear_square(pos, us, p, from);
        set_square(pos, us, p, to);

        // Lose specific castling right (if not already lost)
        pos->castleRooks &= ~(1ULL << from);
    } else {
        // Move piece
        clear_square(pos, us, p, from);
        set_square(pos, us, p, to);

        if (p == PAWN) {
            // reset rule50, and set epSquare
            const int push = push_inc(us);
            pos->rule50 = 0;
            pos->epSquare = to == from + 2 * push
                    && (PawnAttacks[us][from + push] & pos_pieces_cp(pos, them, PAWN))
                ? from + push : NB_SQUARE;

            // handle ep-capture and promotion
            if (to == before->epSquare)
                clear_square(pos, them, p, to - push);
            else if (rank_of(to) == RANK_8 || rank_of(to) == RANK_1) {
                clear_square(pos, us, p, to);
                set_square(pos, us, prom, to);
            }
        } else if (p == KING) {
            // Lose all castling rights
            pos->castleRooks &= ~Rank[us * RANK_8];

            // Castling
            if (bb_test(before->byColor[us], to)) {
                // Capturing our own piece can only be a castling move, encoded KxR
                assert(pos_piece_on(before, to) == ROOK);
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

// Play a null move (ie. switch sides): pos = before + play(null)
void pos_switch(Position *pos, const Position *before)
{
    *pos = *before;
    pos->epSquare = NB_SQUARE;

    pos->turn = opposite(pos->turn);
    pos->key ^= ZobristTurn;
    pos->key ^= ZobristEnPassant[before->epSquare] ^ ZobristEnPassant[pos->epSquare];

    finish(pos);
}

// All pieces
bitboard_t pos_pieces(const Position *pos)
{
    assert(!(pos->byColor[WHITE] & pos->byColor[BLACK]));
    return pos->byColor[WHITE] | pos->byColor[BLACK];
}

// Pieces of color 'c' and type 'p'
bitboard_t pos_pieces_cp(const Position *pos, int c, int p)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p, NB_PIECE);
    return pos->byColor[c] & pos->byPiece[p];
}

// Pieces of color 'c' and type 'p1' or 'p2'
bitboard_t pos_pieces_cpp(const Position *pos, int c, int p1, int p2)
{
    BOUNDS(c, NB_COLOR);
    BOUNDS(p1, NB_PIECE);
    BOUNDS(p2, NB_PIECE);
    return pos->byColor[c] & (pos->byPiece[p1] | pos->byPiece[p2]);
}

// En passant square, in bitboard format
bitboard_t pos_ep_square_bb(const Position *pos)
{
    return pos->epSquare < NB_SQUARE ? 1ULL << pos->epSquare : 0;
}

// Detect insufficient material configuration (draw by chess rules only)
bool pos_insufficient_material(const Position *pos)
{
    return bb_count(pos_pieces(pos)) <= 3 && !pos->byPiece[PAWN] && !pos->byPiece[ROOK]
        && !pos->byPiece[QUEEN];
}

// Square occupied by the king of color 'c'
int pos_king_square(const Position *pos, int c)
{
    assert(bb_count(pos_pieces_cp(pos, c, KING)) == 1);
    return bb_lsb(pos_pieces_cp(pos, c, KING));
}

// Color of piece on square 's'. Square is assumed to be occupied.
int pos_color_on(const Position *pos, int s)
{
    assert(bb_test(pos_pieces(pos), s));
    return bb_test(pos->byColor[WHITE], s) ? WHITE : BLACK;
}

// Piece on square 's'. NB_PIECE if empty.
int pos_piece_on(const Position *pos, int s)
{
    BOUNDS(s, NB_SQUARE);
    return pos->pieceOn[s];
}

// Attackers (or any color) to square 's', using occupancy 'occ' for rook/bishop attacks
bitboard_t pos_attackers_to(const Position *pos, int s, bitboard_t occ)
{
    BOUNDS(s, NB_SQUARE);
    return (pos_pieces_cp(pos, WHITE, PAWN) & PawnAttacks[BLACK][s])
        | (pos_pieces_cp(pos, BLACK, PAWN) & PawnAttacks[WHITE][s])
        | (KnightAttacks[s] & pos->byPiece[KNIGHT])
        | (KingAttacks[s] & pos->byPiece[KING])
        | (bb_rook_attacks(s, occ) & (pos->byPiece[ROOK] | pos->byPiece[QUEEN]))
        | (bb_bishop_attacks(s, occ) & (pos->byPiece[BISHOP] | pos->byPiece[QUEEN]));
}

// Prints the position in ASCII 'art' (for debugging)
void pos_print(const Position *pos)
{
    for (int r = RANK_8; r >= RANK_1; r--) {
        char line[] = ". . . . . . . .";

        for (int f = FILE_A; f <= FILE_H; f++) {
            const int s = square(r, f);
            line[2 * f] = bb_test(pos_pieces(pos), s)
                ? PieceLabel[pos_color_on(pos, s)][pos_piece_on(pos, s)]
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
        char str[3];

        while (b) {
            square_to_string(bb_pop_lsb(&b), str);
            printf(" %s", str);
        }

        puts("");
    }
}
