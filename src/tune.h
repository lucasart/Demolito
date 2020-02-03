#pragma once
#include "types.h"

eval_t pstSeed[NB_PIECE][NB_RANK][NB_FILE / 2];
eval_t Mobility[5][15];

int RookOpen[2];
eval_t BishopPair;
int Ahead;
int Hanging[NB_PIECE];

int RingAttack[4], RingDefense[4];
int CheckAttack[4], CheckDefense[4];
int XRay[4];
int SafetyCurveParam[2];

eval_t Isolated[2];
eval_t Backward[2];
eval_t Doubled;
int Shield[4][NB_RANK];
eval_t Connected[6];
int OurDistance[NB_RANK];
int TheirDistance[NB_RANK];

eval_t PasserBonus[6];
int PasserAdjust[6];
int FreePasser[4];

void tune_declare_all();
void tune_parse_all(const char *fullName, int value);
