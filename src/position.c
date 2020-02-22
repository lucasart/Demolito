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
#include <stdlib.h>
#include <string.h>
#include "bitboard.h"
#include "move.h"
#include "position.h"
#include "util.h"
#include "zobrist.h"

// Sets the position in its empty state (no pieces, white to play, rule50=0, etc.)
static void clear(Position *pos)
{
    memset(pos, 0, sizeof(*pos));
    memset(pos->pieceOn, NB_PIECE, sizeof(pos->pieceOn));
}

// Remove 'piece' of 'color' on 'square'. Such a piece must be there first.
static void clear_square(Position *pos, int color, int piece, int square)
{
    BOUNDS(color, NB_COLOR);
    BOUNDS(piece, NB_PIECE);
    BOUNDS(square, NB_SQUARE);

    bb_clear(&pos->byColor[color], square);
    bb_clear(&pos->byPiece[piece], square);
    pos->pieceOn[square] = NB_PIECE;
    eval_sub(&pos->pst, pst[color][piece][square]);
    pos->key ^= ZobristKey[color][piece][square];

    if (piece <= QUEEN)
        eval_sub(&pos->pieceMaterial[color], Material[piece]);
    else
        pos->kingPawnKey ^= ZobristKey[color][piece][square];
}

// Put 'piece' of 'color' on 'square'. Square must be empty first.
static void set_square(Position *pos, int color, int piece, int square)
{
    BOUNDS(color, NB_COLOR);
    BOUNDS(piece, NB_PIECE);
    BOUNDS(square, NB_SQUARE);

    bb_set(&pos->byColor[color], square);
    bb_set(&pos->byPiece[piece], square);
    pos->pieceOn[square] = piece;
    eval_add(&pos->pst, pst[color][piece][square]);
    pos->key ^= ZobristKey[color][piece][square];

    if (piece <= QUEEN)
        eval_add(&pos->pieceMaterial[color], Material[piece]);
    else
        pos->kingPawnKey ^= ZobristKey[color][piece][square];
}

// Squares attacked by pieces of 'color'
static bitboard_t attacked_by(const Position *pos, int color)
{
    BOUNDS(color, NB_COLOR);

    // King and Knight attacks
    bitboard_t result = KingAttacks[pos_king_square(pos, color)];
    bitboard_t knights = pos_pieces_cp(pos, color, KNIGHT);

    while (knights)
        result |= KnightAttacks[bb_pop_lsb(&knights)];

    // Pawn captures
    bitboard_t pawns = pos_pieces_cp(pos, color, PAWN);
    result |= bb_shift(pawns & ~File[FILE_A], push_inc(color) + LEFT);
    result |= bb_shift(pawns & ~File[FILE_H], push_inc(color) + RIGHT);

    // Sliders
    bitboard_t _occ = pos_pieces(pos) ^ pos_pieces_cp(pos, opposite(color), KING);
    bitboard_t rookMovers = pos_pieces_cpp(pos, color, ROOK, QUEEN);

    while (rookMovers)
        result |= bb_rook_attacks(bb_pop_lsb(&rookMovers), _occ);

    bitboard_t bishopMovers = pos_pieces_cpp(pos, color, BISHOP, QUEEN);

    while (bishopMovers)
        result |= bb_bishop_attacks(bb_pop_lsb(&bishopMovers), _occ);

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

#ifndef NDEBUG
    // Verify piece counts
    for (int color = WHITE; color <= BLACK; color++) {
        for (int piece = KNIGHT; piece <= ROOK; piece++)
            assert(bb_count(pos_pieces_cp(pos, color, piece)) <= 10);

        assert(bb_count(pos_pieces_cp(pos, color, QUEEN)) <= 9);
        assert(bb_count(pos_pieces_cp(pos, color, PAWN)) <= 8);
        assert(bb_count(pos_pieces_cp(pos, color, KING)) == 1);
        assert(bb_count(pos->byColor[color]) <= 16);
    }

    // Verify pawn ranks
    assert(!(pos->byPiece[PAWN] & (Rank[RANK_1] | Rank[RANK_8])));

    // Verify castle rooks
    if (pos->castleRooks) {
        // Castle rooks may only be white rooks on ranks 1, or black rooks on rank 8
        assert(!(pos->castleRooks & ~((Rank[RANK_1] & pos_pieces_cp(pos, WHITE, ROOK))
            | (Rank[RANK_8] & pos_pieces_cp(pos, BLACK, ROOK)))));

        for (int color = WHITE; color <= BLACK; color++) {
            const bitboard_t b = pos->castleRooks & pos->byColor[color];

            if (bb_count(b) == 2)
                // King must be between both rooks (implies king not on edge files)
                assert(Segment[bb_lsb(b)][bb_msb(b)] & pos_pieces_cp(pos, color, KING));
            else if (bb_count(b) == 1)
                // King can't be on edge files
                assert(!(pos_pieces_cp(pos, color, KING) & (File[FILE_A] | File[FILE_H])));
            else
                // No more than 2 castle rooks allowed
                assert(!b);
        }
    }

    // Verify ep square
    if (pos->epSquare != NB_SQUARE) {
        // ep square must be empty
        assert(!bb_test(pos_pieces(pos), pos->epSquare));

        const int rank = rank_of(pos->epSquare);
        const int color = rank == RANK_3 ? WHITE : BLACK;

        // ep square must be on rank 3 or 6
        assert(rank == RANK_3 || rank == RANK_6);

        // square in front must have a pawn of color
        assert(bb_test(pos_pieces_cp(pos, color, PAWN), pos->epSquare + push_inc(color)));

        // square behind must be empty
        assert(!bb_test(pos_pieces(pos), pos->epSquare - push_inc(color)));
    }

    assert(pos->rule50 < 100);
#endif
}

const char *PieceLabel[NB_COLOR] = {"NBRQKP.", "nbrqkp."};

void square_to_string(int square, char *str)
{
    BOUNDS(square, NB_SQUARE + 1);

    if (square == NB_SQUARE)
        *str++ = '-';
    else {
        *str++ = file_of(square) + 'a';
        *str++ = rank_of(square) + '1';
    }

    *str = '\0';
}

int string_to_square(const char *str)
{
    return *str != '-'
        ? square_from(str[1] - '1', str[0] - 'a')
        : NB_SQUARE;
}

// Set position from FEN string
void pos_set(Position *pos, const char *fen)
{
    clear(pos);
    char *str = strdup(fen), *strPos = NULL;
    char *token = strtok_r(str, " ", &strPos);

    // Piece placement
    char ch;
    int file = FILE_A, rank = RANK_8;

    while ((ch = *token++)) {
        if ('1' <= ch && ch <= '8') {
            file += ch -'0';
            assert(file <= NB_FILE);
        } else if (ch == '/') {
            rank--;
            file = FILE_A;
        } else {
            assert(strchr("nbrqkpNBRQKP", ch));
            const bool color = islower(ch);
            set_square(pos, color, strchr(PieceLabel[color], ch) - PieceLabel[color],
                square_from(rank, file++));
        }
    }

    // Turn of play
    token = strtok_r(NULL, " ", &strPos);
    assert(strlen(token) == 1);

    if (token[0] == 'w')
        pos->turn = WHITE;
    else {
        assert(token[0] == 'b');
        pos->turn = BLACK;
        pos->key ^= ZobristTurn;
    }

    // Castling rights
    token = strtok_r(NULL, " ", &strPos);
    assert(strlen(token) <= 4);

    while ((ch = *token++)) {
        rank = isupper(ch) ? RANK_1 : RANK_8;
        ch = toupper(ch);

        if (ch == 'K')
            bb_set(&pos->castleRooks, bb_msb(Rank[rank] & pos->byPiece[ROOK]));
        else if (ch == 'Q')
            bb_set(&pos->castleRooks, bb_lsb(Rank[rank] & pos->byPiece[ROOK]));
        else if ('A' <= ch && ch <= 'H')
            bb_set(&pos->castleRooks, square_from(rank, ch - 'A'));
        else
            assert(ch == '-' && !pos->castleRooks && *token == '\0');
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
    for (int rank = RANK_8; rank >= RANK_1; rank--) {
        int cnt = 0;

        for (int file = FILE_A; file <= FILE_H; file++) {
            const int square = square_from(rank, file);

            if (bb_test(pos_pieces(pos), square)) {
                if (cnt)
                    *fen++ = cnt + '0';

                *fen++ = PieceLabel[pos_color_on(pos, square)][pos_piece_on(pos, square)];
                cnt = 0;
            } else
                cnt++;
        }

        if (cnt)
            *fen++ = cnt + '0';

        *fen++ = rank == RANK_1 ? ' ' : '/';
    }

    // Turn of play
    *fen++ = pos->turn == WHITE ? 'w' : 'b';
    *fen++ = ' ';

    // Castling rights
    if (!pos->castleRooks)
        *fen++ = '-';
    else {
        for (int color = WHITE; color <= BLACK; color++) {
            const bitboard_t b = pos->castleRooks & pos->byColor[color];

            if (b) {
                const int king = pos_king_square(pos, color);

                // Right side castling
                if (b & Ray[king][king + RIGHT])
                    *fen++ = PieceLabel[color][KING];

                // Left side castling
                if (b & Ray[king][king + LEFT])
                    *fen++ = PieceLabel[color][QUEEN];
            }
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
    *pos = *before;

    pos->rule50++;
    pos->epSquare = NB_SQUARE;

    const int us = pos->turn, them = opposite(us);
    const int from = move_from(m), to = move_to(m), prom = move_prom(m);
    const int piece = pos_piece_on(pos, from);
    const int capture = pos_piece_on(pos, to);

    // Capture piece on to square (if any)
    if (capture != NB_PIECE) {
        assert(capture != KING);
        pos->rule50 = 0;

        // Use pos_color_on() instead of them, because we could be playing a KxR castling here
        clear_square(pos, pos_color_on(pos, to), capture, to);

        // Capturing a rook alters corresponding castling right
        pos->castleRooks &= ~(1ULL << to);
    }

    if (piece <= QUEEN) {
        // Move piece
        clear_square(pos, us, piece, from);
        set_square(pos, us, piece, to);

        // Lose specific castling right (if not already lost)
        pos->castleRooks &= ~(1ULL << from);
    } else {
        // Move piece
        clear_square(pos, us, piece, from);
        set_square(pos, us, piece, to);

        if (piece == PAWN) {
            // reset rule50, and set epSquare
            const int push = push_inc(us);
            pos->rule50 = 0;
            pos->epSquare = to == from + 2 * push
                    && (PawnAttacks[us][from + push] & pos_pieces_cp(pos, them, PAWN))
                ? from + push : NB_SQUARE;

            // handle ep-capture and promotion
            if (to == before->epSquare)
                clear_square(pos, them, piece, to - push);
            else if (rank_of(to) == RANK_8 || rank_of(to) == RANK_1) {
                clear_square(pos, us, piece, to);
                set_square(pos, us, prom, to);
            }
        } else if (piece == KING) {
            // Lose all castling rights
            pos->castleRooks &= ~Rank[us * RANK_8];

            // Castling
            if (bb_test(before->byColor[us], to)) {
                // Capturing our own piece can only be a castling move, encoded KxR
                assert(pos_piece_on(before, to) == ROOK);
                const int rank = rank_of(from);

                clear_square(pos, us, KING, to);
                set_square(pos, us, KING, square_from(rank, to > from ? FILE_G : FILE_C));
                set_square(pos, us, ROOK, square_from(rank, to > from ? FILE_F : FILE_D));
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

// Pieces of color 'color' and type 'piece'
bitboard_t pos_pieces_cp(const Position *pos, int color, int piece)
{
    BOUNDS(color, NB_COLOR);
    BOUNDS(piece, NB_PIECE);
    return pos->byColor[color] & pos->byPiece[piece];
}

// Pieces of color 'color' and type 'p1' or 'p2'
bitboard_t pos_pieces_cpp(const Position *pos, int color, int p1, int p2)
{
    BOUNDS(color, NB_COLOR);
    BOUNDS(p1, NB_PIECE);
    BOUNDS(p2, NB_PIECE);
    return pos->byColor[color] & (pos->byPiece[p1] | pos->byPiece[p2]);
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

// Square occupied by the king of color 'color'
int pos_king_square(const Position *pos, int color)
{
    assert(bb_count(pos_pieces_cp(pos, color, KING)) == 1);
    return bb_lsb(pos_pieces_cp(pos, color, KING));
}

// Color of piece on square 'square'. Square is assumed to be occupied.
int pos_color_on(const Position *pos, int square)
{
    assert(bb_test(pos_pieces(pos), square));
    return bb_test(pos->byColor[WHITE], square) ? WHITE : BLACK;
}

// Piece on square 'square'. NB_PIECE if empty.
int pos_piece_on(const Position *pos, int square)
{
    BOUNDS(square, NB_SQUARE);
    return pos->pieceOn[square];
}

// Attackers (or any color) to square 'square', using occupancy 'occ' for rook/bishop attacks
bitboard_t pos_attackers_to(const Position *pos, int square, bitboard_t occ)
{
    BOUNDS(square, NB_SQUARE);
    return (pos_pieces_cp(pos, WHITE, PAWN) & PawnAttacks[BLACK][square])
        | (pos_pieces_cp(pos, BLACK, PAWN) & PawnAttacks[WHITE][square])
        | (KnightAttacks[square] & pos->byPiece[KNIGHT])
        | (KingAttacks[square] & pos->byPiece[KING])
        | (bb_rook_attacks(square, occ) & (pos->byPiece[ROOK] | pos->byPiece[QUEEN]))
        | (bb_bishop_attacks(square, occ) & (pos->byPiece[BISHOP] | pos->byPiece[QUEEN]));
}

// Pinned pieces for the side to move
bitboard_t calc_pins(const Position *pos)
{
    const int us = pos->turn, them = opposite(us);
    const int king = pos_king_square(pos, us);
    bitboard_t pinners = (pos_pieces_cpp(pos, them, ROOK, QUEEN) & RookRays[king])
        | (pos_pieces_cpp(pos, them, BISHOP, QUEEN) & BishopRays[king]);
    bitboard_t result = 0;

    while (pinners) {
        const int square = bb_pop_lsb(&pinners);
        bitboard_t skewered = Segment[king][square] & pos_pieces(pos);
        bb_clear(&skewered, king);
        bb_clear(&skewered, square);

        if (!bb_several(skewered) && (skewered & pos->byColor[us]))
            result |= skewered;
    }

    return result;
}

// Prints the position in ASCII 'art' (for debugging)
void pos_print(const Position *pos)
{
    for (int rank = RANK_8; rank >= RANK_1; rank--) {
        char line[] = ". . . . . . . .";

        for (int file = FILE_A; file <= FILE_H; file++) {
            const int square = square_from(rank, file);
            line[2 * file] = bb_test(pos_pieces(pos), square)
                ? PieceLabel[pos_color_on(pos, square)][pos_piece_on(pos, square)]
                : square == pos->epSquare ? '*' : '.';
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
