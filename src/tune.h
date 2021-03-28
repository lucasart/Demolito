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
#pragma once
#include "types.h"

extern int PieceValue[NB_PIECE];

extern eval_t KnightPstSeed[4+8], RookPstSeed[4+8], QueenPstSeed[4+8];
extern eval_t BishopPstSeed[8][4], KingPstSeed[8][4], PawnPstSeed[6][4];
extern eval_t Mobility[5][15];

extern int RookOpen[2];
extern eval_t BishopPair;
extern int Ahead;
extern int Hanging[NB_PIECE];

extern int RingAttack[6], RingDefense[6];
extern int CheckAttack[4], CheckDefense[4];
extern int XRay[4];
extern int SafetyCurveParam[2];

extern eval_t Isolated[2];
extern eval_t Backward[2];
extern eval_t Doubled;
extern int Shield[4][6];
extern eval_t Connected[6];
extern int Distance[2];

extern eval_t PasserBonus[6];
extern int PasserAdjust[6];
extern int FreePasser[4];

void tune_declare(void);
void tune_parse(const char *fullName, int value);
void tune_refresh(void);

void tune_load(const char *fileName);
void tune_free(void);
void tune_param_list(void);
void tune_param_get(const char *name);
void tune_param_set(const char *name, const char *values);
double tune_linereg(void);
double tune_logitreg(const char *strLambda);
void tune_param_fit(const char *name, int nbIter);
