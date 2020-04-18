#pragma once
#include "types.h"

int PieceValue[NB_PIECE];

eval_t KnightPstSeed[4+8], RookPstSeed[4+8], QueenPstSeed[4+8];
eval_t BishopPstSeed[8][4], KingPstSeed[8][4], PawnPstSeed[6][4];
eval_t Mobility[5][15];

int RookOpen[2];
eval_t BishopPair;
int Ahead;
int Hanging[NB_PIECE];

int RingAttack[6], RingDefense[6];
int CheckAttack[4], CheckDefense[4];
int XRay[4];
int SafetyCurveParam[2];

eval_t Isolated[2];
eval_t Backward[2];
eval_t Doubled;
int Shield[4][6];
eval_t Connected[6];
int Distance[2];

eval_t PasserBonus[6];
int PasserAdjust[6];
int FreePasser[4];

void tune_declare();
void tune_parse(const char *fullName, int value);
void tune_refresh();
