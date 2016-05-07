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

const int Center[NB_FILE] = {-5,-2, 0, 2, 2, 0,-2,-5};

eval_t knight(Rank r, File f)
{
    const eval_t CenterWeight = {10, 3};
    return CenterWeight * (Center[r] + Center[f]);
}

eval_t bishop(Rank r, File f)
{
    const eval_t CenterWeight = {2, 3};
    const eval_t DiagonalWeight = {8, 0};
    const eval_t BackRankWeight = {-20, 0};

    return CenterWeight * (Center[r] + Center[f])
           + DiagonalWeight * (r + f == 7 || r - f == 0)
           + BackRankWeight * (r == RANK_1);
}

eval_t rook(Rank r, File f)
{
    const eval_t FileWeight = {3, 0};
    const eval_t SeventhWeight = {16, 16};

    return FileWeight * Center[f]
           + SeventhWeight * (r == RANK_7);
}

eval_t queen(Rank r, File f)
{
    const eval_t CenterWeight = {0, 4};
    const eval_t BackRankWeight = {-10, 0};

    return CenterWeight * (Center[r] + Center[f])
           + BackRankWeight * (r == RANK_1);
}

eval_t king(Rank r, File f)
{
    const int FileShape[NB_FILE] = { 3, 4, 2, 0, 0, 2, 4, 3};
    const int RankShape[NB_RANK] = { 1, 0,-2,-3,-4,-5,-5,-5};

    const eval_t CenterWeight = {0, 14};
    const eval_t FileWeight = {20, 0};
    const eval_t RankWeight = {14, 0};

    return CenterWeight * (Center[r] + Center[f])
           + FileWeight * FileShape[f]
           + RankWeight * RankShape[r];
}

eval_t pawn(Rank r, File f)
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
    typedef eval_t (*pst_fn)(Rank, File);
    const pst_fn PstFn[NB_PIECE] = {&knight, &bishop, &rook, &queen, &king, &pawn};

    // Calculate PST, based on specialized functions for each piece
    for (Color c = WHITE; c <= BLACK; ++c)
        for (Piece p = KNIGHT; p < NB_PIECE; ++p)
            for (Square s = A1; s <= H8; ++s) {
                const Rank rr = Rank(rank_of(s) ^ (RANK_8 * c));
                const File f = file_of(s);
                table[c][p][s] = (Material[p] + (*PstFn[p])(rr, f)) * (c == WHITE ? 1 : -1);
            }

    // Display, based on verbosity level
    for (int phase = 0; phase < NB_PHASE; phase++)
        for (Color c = WHITE; c < Color(verbosity); ++c)
            for (Piece p = KNIGHT; p < NB_PIECE; ++p) {
                std::cout << (phase == OPENING ? "opening" : "endgame")
                          << PieceLabel[WHITE][p] << std::endl;

                for (Rank r = RANK_8; r >= RANK_1; --r) {
                    for (File f = FILE_A; f <= FILE_H; ++f)
                        std::cout << table[c][p][square(r, f)][phase] << '\t';

                    std::cout << std::endl;
                }
            }
}

}    // namespace pst
