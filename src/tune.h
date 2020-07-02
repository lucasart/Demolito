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

void tune_declare();
void tune_parse(const char *fullName, int value);
void tune_refresh();
