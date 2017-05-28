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
#include "gen.h"
#include "move.h"
#include "position.h"

static move_t *serialize_moves(int from, bitboard_t tss, move_t *mList)
{
    while (tss) {
        const int to = bb_pop_lsb(&tss);
        *mList++ = move_build(from, to, NB_PIECE);
    }

    return mList;
}

move_t *gen_pawn_moves(const Position *pos, move_t *mList, bitboard_t targets, bool subPromotions)
{
    const int us = pos->turn, them = opposite(us);
    const int push = push_inc(us);
    const bitboard_t capturable = pos->byColor[them] | pos_ep_square_bb(pos);
    bitboard_t fss, tss;
    int from;

    // Non promotions
    fss = pos_pieces_cp(pos, us, PAWN) & ~bb_rank(relative_rank(us, RANK_7));

    while (fss) {
        from = bb_pop_lsb(&fss);

        // Calculate to squares: captures, single pushes and double pushes
        tss = PAttacks[us][from] & capturable & targets;

        if (bb_test(~pos_pieces(pos), from + push)) {
            if (bb_test(targets, from + push))
                bb_set(&tss, from + push);

            if (relative_rank_of(us, from) == RANK_2
                    && bb_test(targets & ~pos_pieces(pos), from + 2 * push))
                bb_set(&tss, from + 2 * push);
        }

        // Generate moves
        mList = serialize_moves(from, tss, mList);
    }

    // Promotions
    fss = pos_pieces_cp(pos, us, PAWN) & bb_rank(relative_rank(us, RANK_7));

    while (fss) {
        from = bb_pop_lsb(&fss);

        // Calculate to squares: captures and single pushes
        tss = PAttacks[us][from] & capturable & targets;

        if (bb_test(targets & ~pos_pieces(pos), from + push))
            bb_set(&tss, from + push);

        // Generate promotions
        while (tss) {
            const int to = bb_pop_lsb(&tss);

            if (subPromotions) {
                for (int prom = QUEEN; prom >= KNIGHT; --prom)
                    *mList++ = move_build(from, to, prom);
            } else
                *mList++ = move_build(from, to, QUEEN);
        }
    }

    return mList;
}

move_t *gen_piece_moves(const Position *pos, move_t *mList, bitboard_t targets, bool kingMoves)
{
    const int us = pos->turn;
    bitboard_t fss, tss;
    int from;

    // King moves
    if (kingMoves) {
        from = pos_king_square(pos, us);
        tss = KAttacks[from] & targets;
        mList = serialize_moves(from, tss, mList);
    }

    // Knight moves
    fss = pos_pieces_cp(pos, us, KNIGHT);

    while (fss) {
        from = bb_pop_lsb(&fss);
        tss = NAttacks[from] & targets;
        mList = serialize_moves(from, tss, mList);
    }

    // Rook moves
    fss = pos_pieces_cpp(pos, us, ROOK, QUEEN);

    while (fss) {
        from = bb_pop_lsb(&fss);
        tss = bb_rattacks(from, pos_pieces(pos)) & targets;
        mList = serialize_moves(from, tss, mList);
    }

    // Bishop moves
    fss = pos_pieces_cpp(pos, us, BISHOP, QUEEN);

    while (fss) {
        from = bb_pop_lsb(&fss);
        tss = bb_battacks(from, pos_pieces(pos)) & targets;
        mList = serialize_moves(from, tss, mList);
    }

    return mList;
}

move_t *gen_castling_moves(const Position *pos, move_t *mList)
{
    assert(!pos->checkers);
    const int king = pos_king_square(pos, pos->turn);

    bitboard_t tss = pos->castleRooks & pos->byColor[pos->turn];

    while (tss) {
        const int rook = bb_pop_lsb(&tss);
        const int kto = square(rank_of(rook), rook > king ? FILE_G : FILE_C);
        const int rto = square(rank_of(rook), rook > king ? FILE_F : FILE_D);
        const bitboard_t s = Segment[king][kto] | Segment[rook][rto];

        if (bb_count(s & pos_pieces(pos)) == 2)
            *mList++ = move_build(king, rook, NB_PIECE);
    }

    return mList;
}

move_t *gen_check_escapes(const Position *pos, move_t *mList, bool subPromotions)
{
    assert(pos->checkers);
    bitboard_t ours = pos->byColor[pos->turn];
    const int king = pos_king_square(pos, pos->turn);
    bitboard_t tss;

    // King moves
    tss = KAttacks[king] & ~ours;
    mList = serialize_moves(king, tss, mList);

    if (!bb_several(pos->checkers)) {
        // Single checker
        const int checkerSquare = bb_lsb(pos->checkers);
        const int checkerPiece = pos->pieceOn[checkerSquare];

        // int moves must cover the checking segment for a sliding check, or capture the
        // checker otherwise.
        tss = BISHOP <= checkerPiece && checkerPiece <= QUEEN
              ? Segment[king][checkerSquare]
              : pos->checkers;

        mList = gen_piece_moves(pos, mList, tss & ~ours, false);

        // if checked by a Pawn and epsq is available, then the check must result from a
        // pawn double push, and we also need to consider capturing it en-passant to solve
        // the check.
        if (checkerPiece == PAWN && pos->epSquare < NB_SQUARE)
            bb_set(&tss, pos->epSquare);

        mList = gen_pawn_moves(pos, mList, tss, subPromotions);
    }

    return mList;
}

move_t *gen_all_moves(const Position *pos, move_t *mList)
{
    if (pos->checkers)
        return gen_check_escapes(pos, mList, true);
    else {
        bitboard_t targets = ~pos->byColor[pos->turn];
        move_t *m = mList;

        m = gen_pawn_moves(pos, m, targets, true);
        m = gen_piece_moves(pos, m, targets, true);
        m = gen_castling_moves(pos, m);
        return m;
    }
}

uint64_t gen_perft(const Position *pos, int depth, int ply)
{
    // Do not use bulk-counting. It's faster, but falses profiling results.
    if (depth <= 0)
        return 1;

    uint64_t result = 0;
    Position after;
    move_t mList[MAX_MOVES];
    move_t *end = gen_all_moves(pos, mList);

    for (move_t *m = mList; m != end; m++) {
        if (!move_is_legal(pos, *m))
            continue;

        pos_move(&after, pos, *m);
        const uint64_t subTree = gen_perft(&after, depth - 1, ply + 1);
        result += subTree;

        if (!ply) {
            char str[6];
            move_to_string(pos, *m, str);
            printf("%s\t%" PRIu64 "\n", str, subTree);
        }
    }

    return result;
}
