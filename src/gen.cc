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
#include "gen.h"
#include "move.h"
#include "bitboard.h"

template <bool Promotion>
static move_t *serialize_moves(Move *m, bitboard_t tss, move_t *emList, bool subPromotions = true)
{
    while (tss) {
        m->to = bb_pop_lsb(&tss);

        if (Promotion) {
            if (subPromotions) {
                for (m->prom = QUEEN; m->prom >= KNIGHT; --m->prom)
                    *emList++ = *m;
            } else {
                m->prom = QUEEN;
                *emList++ = *m;
            }
        } else
            *emList++ = *m;
    }

    return emList;
}

move_t *gen_pawn_moves(const Position *pos, move_t *emList, bitboard_t targets, bool subPromotions)
{
    const int us = pos->turn, them = opposite(us);
    const int push = push_inc(us);
    const bitboard_t capturable = pos->byColor[them] | pos_ep_square_bb(pos);
    bitboard_t fss, tss;
    Move m;

    // Non promotions
    fss = pos_pieces_cp(pos, us, PAWN) & ~bb_rank(relative_rank(us, RANK_7));

    while (fss) {
        m.from = bb_pop_lsb(&fss);

        // Calculate to squares: captures, single pushes and double pushes
        tss = PAttacks[us][m.from] & capturable & targets;

        if (bb_test(~pos_pieces(pos), m.from + push)) {
            if (bb_test(targets, m.from + push))
                bb_set(&tss, m.from + push);

            if (relative_rank_of(us, m.from) == RANK_2
                    && bb_test(targets & ~pos_pieces(pos), m.from + 2 * push))
                bb_set(&tss, m.from + 2 * push);
        }

        // Generate moves
        m.prom = NB_PIECE;
        emList = serialize_moves<false>(&m, tss, emList);
    }

    // Promotions
    fss = pos_pieces_cp(pos, us, PAWN) & bb_rank(relative_rank(us, RANK_7));

    while (fss) {
        m.from = bb_pop_lsb(&fss);

        // Calculate to squares: captures and single pushes
        tss = PAttacks[us][m.from] & capturable & targets;

        if (bb_test(targets & ~pos_pieces(pos), m.from + push))
            bb_set(&tss, m.from + push);

        // Generate moves (or promotions)
        emList = serialize_moves<true>(&m, tss, emList, subPromotions);
    }

    return emList;
}

move_t *gen_piece_moves(const Position *pos, move_t *emList, bitboard_t targets, bool kingMoves)
{
    const int us = pos->turn;
    bitboard_t fss, tss;

    Move m;
    m.prom = NB_PIECE;

    // King moves
    if (kingMoves) {
        m.from = pos_king_square(pos, us);
        tss = KAttacks[m.from] & targets;
        emList = serialize_moves<false>(&m, tss, emList);
    }

    // Knight moves
    fss = pos_pieces_cp(pos, us, KNIGHT);

    while (fss) {
        m.from = bb_pop_lsb(&fss);
        tss = NAttacks[m.from] & targets;
        emList = serialize_moves<false>(&m, tss, emList);
    }

    // Rook moves
    fss = pos_pieces_cpp(pos, us, ROOK, QUEEN);

    while (fss) {
        m.from = bb_pop_lsb(&fss);
        tss = bb_rattacks(m.from, pos_pieces(pos)) & targets;
        emList = serialize_moves<false>(&m, tss, emList);
    }

    // Bishop moves
    fss = pos_pieces_cpp(pos, us, BISHOP, QUEEN);

    while (fss) {
        m.from = bb_pop_lsb(&fss);
        tss = bb_battacks(m.from, pos_pieces(pos)) & targets;
        emList = serialize_moves<false>(&m, tss, emList);
    }

    return emList;
}

move_t *gen_castling_moves(const Position *pos, move_t *emList)
{
    assert(!pos->checkers);
    Move m;
    m.from = pos_king_square(pos, pos->turn);
    m.prom = NB_PIECE;

    bitboard_t tss = pos->castleRooks & pos->byColor[pos->turn];

    while (tss) {
        m.to = bb_pop_lsb(&tss);
        const int kto = square(rank_of(m.to), m.to > m.from ? FILE_G : FILE_C);
        const int rto = square(rank_of(m.to), m.to > m.from ? FILE_F : FILE_D);
        const bitboard_t s = Segment[m.from][kto] | Segment[m.to][rto];

        if (bb_count(s & pos_pieces(pos)) == 2)
            *emList++ = m;
    }

    return emList;
}

move_t *gen_check_escapes(const Position *pos, move_t *emList, bool subPromotions)
{
    assert(pos->checkers);
    bitboard_t ours = pos->byColor[pos->turn];
    const int king = pos_king_square(pos, pos->turn);
    bitboard_t tss;
    Move m;

    // King moves
    tss = KAttacks[king] & ~ours;
    m.from = king;
    m.prom = NB_PIECE;
    emList = serialize_moves<false>(&m, tss, emList);

    if (!bb_several(pos->checkers)) {
        // Single checker
        const int checkerSquare = bb_lsb(pos->checkers);
        const int checkerPiece = pos->pieceOn[checkerSquare];

        // int moves must cover the checking segment for a sliding check, or capture the
        // checker otherwise.
        tss = BISHOP <= checkerPiece && checkerPiece <= QUEEN
              ? Segment[king][checkerSquare]
              : pos->checkers;

        emList = gen_piece_moves(pos, emList, tss & ~ours, false);

        // if checked by a Pawn and epsq is available, then the check must result from a
        // pawn double push, and we also need to consider capturing it en-passant to solve
        // the check.
        if (checkerPiece == PAWN && pos->epSquare < NB_SQUARE)
            bb_set(&tss, pos->epSquare);

        emList = gen_pawn_moves(pos, emList, tss, subPromotions);
    }

    return emList;
}

move_t *gen_all_moves(const Position *pos, move_t *emList)
{
    if (pos->checkers)
        return gen_check_escapes(pos, emList);
    else {
        bitboard_t targets = ~pos->byColor[pos->turn];
        move_t *em = emList;

        em = gen_pawn_moves(pos, em, targets);
        em = gen_piece_moves(pos, em, targets);
        em = gen_castling_moves(pos, em);
        return em;
    }
}

template <bool Root>
uint64_t gen_perft(const Position *pos, int depth)
{
    // Do not use bulk-counting. It's faster, but falses profiling results.
    if (depth <= 0)
        return 1;

    uint64_t result = 0;
    Position after;
    move_t emList[MAX_MOVES];
    move_t *end = gen_all_moves(pos, emList);

    for (move_t *em = emList; em != end; em++) {
        const Move m(*em);

        if (!move_is_legal(pos, &m))
            continue;

        pos_move(&after, pos, m);
        const uint64_t subTree = gen_perft<false>(&after, depth - 1);
        result += subTree;

        if (Root)
            printf("%s\t%" PRIu64 "\n", move_to_string(pos, &m).c_str(), subTree);
    }

    return result;
}

template uint64_t gen_perft<true>(const Position *pos, int depth);
