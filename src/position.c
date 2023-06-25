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
#include "position.h"
#include "bitboard.h"
#include "tune.h"
#include "uci.h"
#include "util.h"
#include "zobrist.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Sets the position in its empty state (no pieces, white to play, rule50=0, etc.)
static void clear(Position *pos) {
    *pos = (Position){0};
    memset(pos->pieceOn, NB_PIECE, sizeof(pos->pieceOn));
}

// Remove 'piece' of 'color' on 'square'. Such a piece must be there first.
static void clear_square(Position *pos, int color, int piece, int square) {
    BOUNDS(color, NB_COLOR);
    BOUNDS(piece, NB_PIECE);
    BOUNDS(square, NB_SQUARE);

    bb_clear(&pos->byColor[color], square);
    bb_clear(&pos->byPiece[piece], square);
    pos->pieceOn[square] = NB_PIECE;
    eval_sub(&pos->pst, PST[color][piece][square]);
    pos->key ^= ZobristKey[color][piece][square];

    if (piece <= QUEEN)
        pos->pieceMaterial[color] -= PieceValue[piece];
    else
        pos->kingPawnKey ^= ZobristKey[color][piece][square];
}

// Put 'piece' of 'color' on 'square'. Square must be empty first.
static void set_square(Position *pos, int color, int piece, int square) {
    BOUNDS(color, NB_COLOR);
    BOUNDS(piece, NB_PIECE);
    BOUNDS(square, NB_SQUARE);

    bb_set(&pos->byColor[color], square);
    bb_set(&pos->byPiece[piece], square);
    pos->pieceOn[square] = (uint8_t)piece;
    eval_add(&pos->pst, PST[color][piece][square]);
    pos->key ^= ZobristKey[color][piece][square];

    if (piece <= QUEEN)
        pos->pieceMaterial[color] += PieceValue[piece];
    else
        pos->kingPawnKey ^= ZobristKey[color][piece][square];
}

// Squares attacked by pieces of 'color'
static bitboard_t attacked_by(const Position *pos, int color) {
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
static void finish(Position *pos) {
    const int us = pos->turn, them = opposite(us);
    const int king = pos_king_square(pos, us);

    pos->attacked = attacked_by(pos, them);
    pos->checkers = bb_test(pos->attacked, king)
                        ? pos_attackers_to(pos, king, pos_pieces(pos)) & pos->byColor[them]
                        : 0;

#ifndef NDEBUG
    // Verify that byColor[] and byPiece[] do not collide, and are consistent
    bitboard_t all = 0;

    for (int piece = KNIGHT; piece <= PAWN; piece++) {
        assert(!(pos->byPiece[piece] & all));
        all |= pos->byPiece[piece];
    }

    assert(!(pos->byColor[WHITE] & pos->byColor[BLACK]));
    assert(all == (pos->byColor[WHITE] | pos->byColor[BLACK]));

    // Verify piece counts
    for (int color = WHITE; color <= BLACK; color++) {
        assert(bb_count(pos_pieces_cpp(pos, color, KNIGHT, PAWN)) <= 10);
        assert(bb_count(pos_pieces_cpp(pos, color, BISHOP, PAWN)) <= 10);
        assert(bb_count(pos_pieces_cpp(pos, color, ROOK, PAWN)) <= 10);
        assert(bb_count(pos_pieces_cpp(pos, color, QUEEN, PAWN)) <= 9);
        assert(bb_count(pos_pieces_cp(pos, color, PAWN)) <= 8);
        assert(bb_count(pos_pieces_cp(pos, color, KING)) == 1);
        assert(bb_count(pos->byColor[color]) <= 16);
    }

    // Verify pawn ranks
    assert(!(pos->byPiece[PAWN] & (Rank[RANK_1] | Rank[RANK_8])));

    // Verify castle rooks
    if (pos->castleRooks) {
        assert(!(pos->castleRooks & ~((Rank[RANK_1] & pos_pieces_cp(pos, WHITE, ROOK)) |
                                      (Rank[RANK_8] & pos_pieces_cp(pos, BLACK, ROOK)))));

        for (int color = WHITE; color <= BLACK; color++) {
            const bitboard_t b = pos->castleRooks & pos->byColor[color];

            if (bb_count(b) == 2)
                assert(Segment[bb_lsb(b)][bb_msb(b)] & pos_pieces_cp(pos, color, KING));
            else if (bb_count(b) == 1)
                assert(!(pos_pieces_cp(pos, color, KING) & (File[FILE_A] | File[FILE_H])));
            else
                assert(!b);
        }
    }

    // Verify ep square
    if (pos->epSquare != NB_SQUARE) {
        const int rank = rank_of(pos->epSquare);
        const int color = rank == RANK_3 ? WHITE : BLACK;

        assert(color != pos->turn);
        assert(!bb_test(pos_pieces(pos), pos->epSquare));
        assert(rank == RANK_3 || rank == RANK_6);
        assert(bb_test(pos_pieces_cp(pos, color, PAWN), pos->epSquare + push_inc(color)));
        assert(!bb_test(pos_pieces(pos), pos->epSquare - push_inc(color)));
    }

    // Verify key, kingPawnKey, pieceMaterial[], pst, pieceOn[]
    bitboard_t key = 0, kingPawnKey = 0;
    int pieceMaterial[NB_COLOR] = {0};
    eval_t pst = {0, 0};

    for (int color = WHITE; color <= BLACK; color++)
        for (int piece = KNIGHT; piece <= PAWN; piece++) {
            bitboard_t b = pos_pieces_cp(pos, color, piece);

            while (b) {
                const int square = bb_pop_lsb(&b);
                assert(pos->pieceOn[square] == piece);

                key ^= ZobristKey[color][piece][square];
                eval_add(&pst, PST[color][piece][square]);

                if (piece <= QUEEN)
                    pieceMaterial[color] += PieceValue[piece];
                else
                    kingPawnKey ^= ZobristKey[color][piece][square];
            }
        }

    assert(kingPawnKey == pos->kingPawnKey);
    key ^= ZobristEnPassant[pos->epSquare] ^ (pos->turn == BLACK ? ZobristTurn : 0) ^
           zobrist_castling(pos->castleRooks);
    assert(pos->key == key);
    assert(pieceMaterial[WHITE] == pos->pieceMaterial[WHITE]);
    assert(pieceMaterial[BLACK] == pos->pieceMaterial[BLACK]);
    assert(eval_eq(pst, pos->pst));

    // Verify turn and rule50
    assert(pos->turn == WHITE || pos->turn == BLACK);
    assert(pos->rule50 < 100);
#endif
}

const char *PieceLabel[NB_COLOR] = {"NBRQKP.", "nbrqkp."};

void square_to_string(int square, char *str) {
    BOUNDS(square, NB_SQUARE + 1);

    if (square == NB_SQUARE)
        *str++ = '-';
    else {
        *str++ = 'a' + (char)file_of(square);
        *str++ = '1' + (char)rank_of(square);
    }

    *str = '\0';
}

int string_to_square(const char *str) {
    return *str != '-' ? square_from(str[1] - '1', str[0] - 'a') : NB_SQUARE;
}

// Set position from FEN string
void pos_set(Position *pos, const char *fen) {
    clear(pos);
    char *str = strdup(fen), *strPos = NULL;
    char *token = strtok_r(str, " ", &strPos);

    // Piece placement
    char ch = 0;
    int file = FILE_A, rank = RANK_8;

    while ((ch = *token++)) {
        if ('1' <= ch && ch <= '8') {
            file += ch - '0';
            assert(file <= NB_FILE);
        } else if (ch == '/') {
            rank--;
            file = FILE_A;
        } else {
            assert(strchr("nbrqkpNBRQKP", ch));
            const bool color = islower(ch);
            set_square(pos, color, (int)(strchr(PieceLabel[color], ch) - PieceLabel[color]),
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
        const bitboard_t ourRooks = pos_pieces_cp(pos, rank / RANK_8, ROOK);
        ch = (char)toupper(ch);

        if (ch == 'K')
            bb_set(&pos->castleRooks, bb_msb(Rank[rank] & ourRooks));
        else if (ch == 'Q')
            bb_set(&pos->castleRooks, bb_lsb(Rank[rank] & ourRooks));
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
void pos_get(const Position *pos, char *fen) {
    // Piece placement
    for (int rank = RANK_8; rank >= RANK_1; rank--) {
        int cnt = 0;

        for (int file = FILE_A; file <= FILE_H; file++) {
            const int square = square_from(rank, file);

            if (bb_test(pos_pieces(pos), square)) {
                if (cnt)
                    *fen++ = '0' + (char)cnt;

                *fen++ = PieceLabel[pos_color_on(pos, square)][pos->pieceOn[square]];
                cnt = 0;
            } else
                cnt++;
        }

        if (cnt)
            *fen++ = '0' + (char)cnt;

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
void pos_move(Position *pos, const Position *before, move_t m) {
    *pos = *before;

    pos->rule50++;
    pos->epSquare = NB_SQUARE;

    const int us = pos->turn, them = opposite(us);
    const int from = move_from(m), to = move_to(m), prom = move_prom(m);
    const int piece = pos->pieceOn[from];
    const int capture = pos->pieceOn[to];

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

            // Set ep square upon double push, only if catpturably by enemy pawns
            if (to == from + 2 * push &&
                (PawnAttacks[us][from + push] & pos_pieces_cp(pos, them, PAWN)))
                pos->epSquare = from + push;

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
                assert(capture == ROOK);
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
void pos_switch(Position *pos, const Position *before) {
    *pos = *before;
    pos->epSquare = NB_SQUARE;

    pos->turn = opposite(pos->turn);
    pos->key ^= ZobristTurn;
    pos->key ^= ZobristEnPassant[before->epSquare] ^ ZobristEnPassant[pos->epSquare];

    finish(pos);
}

// All pieces
bitboard_t pos_pieces(const Position *pos) {
    assert(!(pos->byColor[WHITE] & pos->byColor[BLACK]));
    return pos->byColor[WHITE] | pos->byColor[BLACK];
}

// Pieces of color 'color' and type 'piece'
bitboard_t pos_pieces_cp(const Position *pos, int color, int piece) {
    BOUNDS(color, NB_COLOR);
    BOUNDS(piece, NB_PIECE);
    return pos->byColor[color] & pos->byPiece[piece];
}

// Pieces of color 'color' and type 'p1' or 'p2'
bitboard_t pos_pieces_cpp(const Position *pos, int color, int p1, int p2) {
    BOUNDS(color, NB_COLOR);
    BOUNDS(p1, NB_PIECE);
    BOUNDS(p2, NB_PIECE);
    return pos->byColor[color] & (pos->byPiece[p1] | pos->byPiece[p2]);
}

// En passant square, in bitboard format
bitboard_t pos_ep_square_bb(const Position *pos) {
    return pos->epSquare < NB_SQUARE ? 1ULL << pos->epSquare : 0;
}

// Detect insufficient material configuration (draw by chess rules only)
bool pos_insufficient_material(const Position *pos) {
    return bb_count(pos_pieces(pos)) <= 3 && !pos->byPiece[PAWN] && !pos->byPiece[ROOK] &&
           !pos->byPiece[QUEEN];
}

// Square occupied by the king of color 'color'
int pos_king_square(const Position *pos, int color) {
    assert(bb_count(pos_pieces_cp(pos, color, KING)) == 1);
    return bb_lsb(pos_pieces_cp(pos, color, KING));
}

// Color of piece on square 'square'. Square is assumed to be occupied.
int pos_color_on(const Position *pos, int square) {
    assert(bb_test(pos_pieces(pos), square));
    return bb_test(pos->byColor[WHITE], square) ? WHITE : BLACK;
}

// Attackers (or any color) to square 'square', using occupancy 'occ' for rook/bishop attacks
bitboard_t pos_attackers_to(const Position *pos, int square, bitboard_t occ) {
    BOUNDS(square, NB_SQUARE);
    return (pos_pieces_cp(pos, WHITE, PAWN) & PawnAttacks[BLACK][square]) |
           (pos_pieces_cp(pos, BLACK, PAWN) & PawnAttacks[WHITE][square]) |
           (KnightAttacks[square] & pos->byPiece[KNIGHT]) |
           (KingAttacks[square] & pos->byPiece[KING]) |
           (bb_rook_attacks(square, occ) & (pos->byPiece[ROOK] | pos->byPiece[QUEEN])) |
           (bb_bishop_attacks(square, occ) & (pos->byPiece[BISHOP] | pos->byPiece[QUEEN]));
}

// Pinned pieces for the side to move
bitboard_t calc_pins(const Position *pos) {
    const int us = pos->turn, them = opposite(us);
    const int king = pos_king_square(pos, us);
    bitboard_t pinners = (pos_pieces_cpp(pos, them, ROOK, QUEEN) & bb_rook_attacks(king, 0)) |
                         (pos_pieces_cpp(pos, them, BISHOP, QUEEN) & bb_bishop_attacks(king, 0));
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

bool pos_move_is_capture(const Position *pos, move_t m) {
    const int from = move_from(m), to = move_to(m);
    return bb_test(pos->byColor[opposite(pos->turn)], to) ||
           (pos->pieceOn[from] == PAWN && (to == pos->epSquare || move_prom(m) < NB_PIECE));
}

bool pos_move_is_castling(const Position *pos, move_t m) {
    return bb_test(pos->byColor[pos->turn], move_to(m));
}

void pos_move_to_string(const Position *pos, move_t m, char *str) {
    const int from = move_from(m), to = move_to(m), prom = move_prom(m);

    if (!(from | to | prom)) {
        strcpy(str, "0000");
        return;
    }

    const int _to = !uciChess960 && pos_move_is_castling(pos, m)
                        ? (to > from ? from + 2 : from - 2) // e1h1 -> e1g1, e1a1 -> e1c1
                        : to;

    square_to_string(from, str);
    square_to_string(_to, str + 2);

    if (prom < NB_PIECE) {
        str[4] = PieceLabel[BLACK][prom];
        str[5] = '\0';
    }
}

move_t pos_string_to_move(const Position *pos, const char *str) {
    const int prom =
        str[4] ? (int)(strchr(PieceLabel[BLACK], str[4]) - PieceLabel[BLACK]) : NB_PIECE;
    const int from = square_from(str[1] - '1', str[0] - 'a');
    int to = square_from(str[3] - '1', str[2] - 'a');

    if (!uciChess960 && pos->pieceOn[from] == KING) {
        if (to == from + 2) // e1g1 -> e1h1
            to++;
        else if (to == from - 2) // e1c1 -> e1a1
            to -= 2;
    }

    return move_build(from, to, prom);
}

// Static Exchange Evaluator
int pos_see(const Position *pos, move_t m) {
    const int from = move_from(m), to = move_to(m), prom = move_prom(m);
    int us = pos->turn;
    bitboard_t occ = pos_pieces(pos);

    // General case
    int gain[32];
    int moved = pos->pieceOn[from];
    gain[0] = PieceValue[pos->pieceOn[to]];
    bb_clear(&occ, from);

    // Special cases
    if (moved == PAWN) {
        if (to == pos->epSquare) {
            bb_clear(&occ, to - push_inc(us));
            gain[0] = PieceValue[moved];
        } else if (prom < NB_PIECE) {
            moved = prom;
            gain[0] += PieceValue[moved] - PieceValue[PAWN];
        }
    }

    // Easy case: to is not defended (~41% of the time)
    if (!bb_test(pos->attacked, to))
        return gain[0];

    bitboard_t attackers = pos_attackers_to(pos, to, occ);
    bitboard_t ourAttackers;

    int idx = 0;

    // Loop side by side and play (any) LVA recapture (~1.6 iterations on average)
    while (us = opposite(us), ourAttackers = attackers & pos->byColor[us]) {
        // Find least valuable attacker (LVA)
        int lva = PAWN;

        if (!(ourAttackers & pos_pieces_cp(pos, us, PAWN)))
            for (lva = KNIGHT; lva <= KING; lva++)
                if (ourAttackers & pos_pieces_cp(pos, us, lva))
                    break;

        // Remove the LVA
        bb_clear(&occ, bb_lsb(ourAttackers & pos_pieces_cp(pos, us, lva)));

        // Remove attackers we've already done. Purposely omit look through attacks (no elo gain).
        attackers &= occ;

        // Add the new entry to the gain[] array
        idx++;
        assert(idx < 32);
        gain[idx] = PieceValue[moved] - gain[idx - 1];

        moved = lva;
    }

    do {
        if (-gain[idx] < gain[idx - 1])
            gain[idx - 1] = -gain[idx];
    } while (--idx);

    return gain[0];
}

// Prints the position in ASCII 'art' (for debugging)
void pos_print(const Position *pos) {
    for (int rank = RANK_8; rank >= RANK_1; rank--) {
        char line[] = ". . . . . . . .";

        for (int file = FILE_A; file <= FILE_H; file++) {
            const int square = square_from(rank, file);
            line[2 * file] = bb_test(pos_pieces(pos), square)
                                 ? PieceLabel[pos_color_on(pos, square)][pos->pieceOn[square]]
                             : square == pos->epSquare ? '*'
                                                       : '.';
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
