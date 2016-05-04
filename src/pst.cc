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
#include <iostream>
#include <algorithm>
#include "pst.h"

namespace pst {

eval_t table[NB_COLOR][NB_PIECE][NB_SQUARE];

const int Center[8] = {-5,-2, 0, 2, 2, 0,-2,-5};

eval_t knight(int r, int f)
{
    const eval_t CenterWeight = {10, 3};
    return CenterWeight * (Center[r] + Center[f]);
}

eval_t bishop(int r, int f)
{
    const eval_t CenterWeight = {2, 3};
    const eval_t DiagonalWeight = {8, 0};
    const eval_t BackRankWeight = {-20, 0};

    return CenterWeight * (Center[r] + Center[f])
           + DiagonalWeight * (7 == r + f || r == f)
           + BackRankWeight * (r == RANK_1);
}

eval_t rook(int r, int f)
{
    const eval_t FileWeight = {3, 0};
    const eval_t SeventhWeight = {16, 16};

    return FileWeight * Center[f]
           + SeventhWeight * (r == RANK_7);
}

eval_t queen(int r, int f)
{
    const eval_t CenterWeight = {0, 4};
    const eval_t BackRankWeight = {-10, 0};

    return CenterWeight * (Center[r] + Center[f])
           + BackRankWeight * (r == RANK_1);
}

eval_t king(int r, int f)
{
    const int FileShape[8] = { 3, 4, 2, 0, 0, 2, 4, 3};
    const int RankShape[8] = { 1, 0,-2,-3,-4,-5,-5,-5};

    const eval_t CenterWeight = {0, 14};
    const eval_t FileWeight = {20, 0};
    const eval_t RankWeight = {14, 0};

    return CenterWeight * (Center[r] + Center[f])
           + FileWeight * FileShape[f]
           + RankWeight * RankShape[r];
}

eval_t pawn(int r, int f)
{
    const eval_t PCenter = {36, 0};
    const eval_t P7Rank = {100, 100};    // FIXME: replace by proper passed pawn eval
    eval_t e = {0, 0};

    if (f == FILE_D || f == FILE_E) {
        if (r == RANK_3 || r == RANK_5)
            e += PCenter / 2;
        else if (r == RANK_4)
            e += PCenter;
    }

    if (r == RANK_7)
        e += P7Rank;

    return e;
}

void init(int verbosity)
{
    typedef eval_t (*pst_fn)(int, int);
    const pst_fn PstFn[NB_PIECE] = {&knight, &bishop, &rook, &queen, &king, &pawn};

    // Calculate PST, based on specialized functions for each piece
    for (Color c = WHITE; c <= BLACK; ++c)
        for (Piece p = KNIGHT; p < NB_PIECE; ++p)
            for (int sq = 0; sq < NB_SQUARE; sq++) {
                const int rr = rank_of(sq) ^ (RANK_8 * c), f = file_of(sq);
                table[c][p][sq] = (Material[p] + (*PstFn[p])(rr, f)) * (c == WHITE ? 1 : -1);
            }

    // Display, based on verbosity level
    for (int phase = 0; phase < NB_PHASE; phase++)
        for (Color c = WHITE; c < Color(verbosity); ++c)
            for (Piece p = KNIGHT; p < NB_PIECE; ++p) {
                std::cout << (phase == OPENING ? "opening" : "endgame")
                          << PieceLabel[WHITE][p] << std::endl;

                for (int r = RANK_8; r >= RANK_1; r--) {
                    for (int f = FILE_A; f <= FILE_H; f++)
                        std::cout << table[c][p][square(r, f)][phase] << '\t';

                    std::cout << std::endl;
                }
            }
}

}    // namespace pst
