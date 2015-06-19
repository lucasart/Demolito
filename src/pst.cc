/*
 * Demolito, a UCI chess engine.
 * Copyright 2015 Lucas Braesch.
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

const int CenterShape[8] = {-2,-1, 0, 1, 1, 0,-1,-2};

eval_t knight(int r, int f)
{
	const eval_t CenterWeight = {10, 3};
	return (CenterShape[r] + CenterShape[f]) * CenterWeight;
}

eval_t bishop(int r, int f)
{
	const eval_t CenterWeight   = {2, 3};
	const eval_t DiagonalWeight = {4, 0};
	const eval_t BackRankWeight = {-10, 0};

	return (CenterShape[r] + CenterShape[f]) * CenterWeight
		+ DiagonalWeight * (7 == r + f || r == f)
		+ BackRankWeight * (r == RANK_1);
}

eval_t rook(int r, int f)
{
	const eval_t FileWeight    = {3, 0};
	const eval_t SeventhWeight = {8, 8};

	return CenterShape[f] * FileWeight
		+ (r == RANK_7) * SeventhWeight;
}

eval_t queen(int r, int f)
{
	const eval_t CenterWeight   = {0, 4};
	const eval_t BackRankWeight = {-5, 0};

	return (CenterShape[r] + CenterShape[f]) * CenterWeight
		+ BackRankWeight * (r == RANK_1);
}

eval_t king(int r, int f)
{
	const int FileShape[8] = { 3, 4, 2, 0, 0, 2, 4, 3};
	const int RankShape[8] = { 1, 0,-2,-3,-4,-5,-5,-5};

	const eval_t CenterWeight = {0, 14};
	const eval_t FileWeight   = {10, 0};
	const eval_t RankWeight   = {7, 0};

	return CenterWeight * (CenterShape[r] + CenterShape[f])
		+ FileWeight * FileShape[f]
		+ RankWeight * RankShape[r];
}

eval_t pawn(int r, int f)
{
	const eval_t CenterWeight = {8, 0};
	return CenterWeight * CenterShape[f];
}

void init(int verbosity)
{
	typedef eval_t (*pst_fn)(int, int);
	const pst_fn PstFn[NB_PIECE] = {&knight, &bishop, &rook, &queen, &king, &pawn};

	for (int color = 0; color < NB_COLOR; color++)
	for (int piece = 0; piece < NB_PIECE; piece++)
	for (int sq = 0; sq < NB_SQUARE; sq++) {
		const int rr = rank_of(sq) ^ (RANK_8 * color), f = file_of(sq);
		table[color][piece][sq] = (color == WHITE ? 1 : -1)
			* (Material[piece] + (*PstFn[piece])(rr, f));
	}

	for (int phase = 0; phase < NB_PHASE; phase++)
	for (int color = 0; color < verbosity; color++)
	for (int piece = 0; piece < NB_PIECE; piece++) {
		std::cout << (phase == OPENING ? "opening" : "endgame")
			<< PieceLabel[WHITE][piece] << std::endl;
		for (int r = RANK_8; r >= RANK_1; r--) {
			for (int f = FILE_A; f <= FILE_H; f++)
				std::cout << table[color][piece][square(r, f)][phase] << '\t';
			std::cout << std::endl;
		}
	}
}

}	// namespace pst
