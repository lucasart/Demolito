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
#include <string.h>
#include "eval.h"
#include "pst.h"
#include "search.h"
#include "tune.h"

int PieceValue[NB_PIECE] = {640, 640, 1046, 1980, MATE, 179};

eval_t KnightPstSeed[4+8] = {
    {-56, -19}, {-32, -14}, {-15, -4}, {7, 4},
    {-31, -12}, {-11, -2}, {15, 1}, {33, 10}, {28, 9}, {13, 3}, {-9, -1}, {-37, -7}
};

eval_t RookPstSeed[4+8] = {
    {-13, 0}, {-2, 3}, {2, 4}, {5, 4},
    {-6, -6}, {-2, -7}, {-2, -2}, {-6, -3}, {0, 0}, {-2, 0}, {13, 12}, {-1, -3}
};

eval_t QueenPstSeed[4+8] = {
    {-3, -28}, {-4, -14}, {-2, -3}, {1, 2},
    {-6, -15}, {5, -8}, {4, 8}, {0, 17}, {-2, 13}, {2, 4}, {-2, -4}, {1, -13}
};

eval_t BishopPstSeed[8][4] = {
    {{ -6,-2}, {-2, 2}, {-11, 4}, { 1,-5}},
    {{ -3,-3}, {15, 2}, {  9, 9}, {-9, 5}},
    {{ -2,-6}, { 8,-2}, {  3, 4}, {13, 3}},
    {{ 19,-7}, {-1, 7}, {  0, 0}, {-1, 4}},
    {{-11,-1}, {-2,-2}, {  0, 2}, {-2, 1}},
    {{ -3, 5}, {-4,-1}, {  1, 1}, { 0, 6}},
    {{  2,-3}, {-5,-3}, {  3,-1}, {-5, 4}},
    {{  1,10}, {-8, 1}, {  1, 4}, { 5,-3}}
};

eval_t KingPstSeed[8][4] = {
    {{ 59,-138}, {91,-102}, { 59,-64}, { 26,-56}},
    {{ 54, -83}, {76, -53}, { 44,-17}, {  4, -3}},
    {{ 32, -69}, {50, -17}, {  7,  1}, {-46, 26}},
    {{ 17, -24}, {24,   2}, {  1, 23}, {-40, 43}},
    {{  2, -27}, {15,   3}, {-19, 26}, {-53, 59}},
    {{ -8, -72}, {16, -33}, {-29,  1}, {-56, 20}},
    {{-16, -75}, {17, -39}, {-12,-29}, {-53,  1}},
    {{ -4,-119}, { 3,-115}, {-21,-57}, {-55,-27}}
};

eval_t PawnPstSeed[6][4] = {
    {{-5,-4}, { 8, 3}, { 7, 4}, {-5, 6}},
    {{-9,-4}, { 3,-1}, {-5, 0}, {11,-1}},
    {{-8, 6}, { 3, 0}, { 5,-1}, {39,-6}},
    {{ 2, 3}, {-1, 4}, { 4, 3}, {22,-1}},
    {{-1, 8}, {12, 3}, {-9,-4}, {-2, 5}},
    {{-6,-3}, { 2, 1}, {-4,-2}, { 2, 1}},
};

eval_t Mobility[5][15] = {
    // Knight
    {{-38, -48}, {-18, -30}, {-15, -21}, {4, 0}, {9, 16}, {20, 34}, {34, 28}, {35, 46},
        {29, 47}},
    // Bishop
    {{-66, -70}, {-42, -35}, {-16, -30}, {-7, -19}, {12, -2}, {24, 16}, {37, 33}, {27, 50},
        {41, 55}, {45, 65}, {55, 37}, {71, 65}, {71, 56}, {120, 95}},
     // Rook
    {{-60, -46}, {-33, -51}, {-19, -28}, {-6, -22}, {-5, -3}, {-5, 1}, {9, 2}, {19, 5},
        {27, 19}, {41, 38}, {57, 33}, {56, 40}, {48, 43}, {57, 48}, {72, 56}},
    // Queen diagonal
    {{-27, -41}, {-9, -29}, {-12, -4}, {-4, -8}, {0, -1}, {11, 13}, {11, 23}, {17, 41},
        {13, 13}, {23, 47}, {28, 50}, {34, 24}, {16, 32}, {24, 87}},
    // Queen orthogonal
    {{-24, -64}, {-19, -25}, {-16, -11}, {-9, -4}, {-2, -10}, {4, -10}, {2, 16}, {9, 14},
        {13, 14}, {21, 26}, {8, 37}, {21, 43}, {25, 48}, {19, 45}, {24, 54}}
};

int RookOpen[2] = {20, 33};  // 0: semi-open, 1: fully-open
eval_t BishopPair = {77, 122};
int Ahead = 20;
int Hanging[NB_PIECE] = {119, 71, 118, 233, 0, 42};

int RingAttack[NB_PIECE] = {38, 51, 77, 75, 0, 20};
int RingDefense[NB_PIECE] = {21, 29, 52, 45, 0, 0};
int CheckAttack[4] = {84, 102, 108, 106};
int CheckDefense[4] = {31, 55, 40, 48};
int XRay[4] = {0, 83, 116, 85};
int SafetyCurveParam[2] = {1062, 800};

eval_t Isolated[2] = {{15, 25}, {41, 25}};
eval_t Backward[2] = {{12, 18}, {37, 15}};
eval_t Doubled = {28, 36};
int Shield[4][6] = {
    {21, 19, 8, 12, 17, 11},
    {31, 23, 5, 7, 5, 3},
    {25, 17, 12, 14, 17, 13},
    {25, 18, 16, 10, 7, 6}
};
eval_t Connected[] = {{8, -4}, {18, 2}, {20, 7}, {42, 21}, {34, 58}, {48, 68}};
int Distance[2] = {9, 9};

eval_t PasserBonus[6] = {{-3, 7}, {2, 15}, {18, 20}, {57, 61}, {150, 155}, {264, 270}};
int PasserAdjust[6] = {-1, 4, 12, 50, 72, 91};
int FreePasser[4] = {13, 14, 35, 97};

enum {NAME_MAX_CHAR = 64};

typedef struct {
    char name[NAME_MAX_CHAR];
    void *values;
    int count;
} Entry;

Entry Entries[] = {
    {"PieceValue", PieceValue, NB_PIECE},

    {"KnightPstSeed", KnightPstSeed, 12 * 2},
    {"RookPstSeed", RookPstSeed, 12 * 2},
    {"QueenPstSeed", QueenPstSeed, 12 * 2},

    {"BishopPstSeed", BishopPstSeed, 8 * 4 * 2},
    {"KingPstSeed", KingPstSeed, 8 * 4 * 2},
    {"PawnPstSeed", PawnPstSeed, 6 * 4 * 2},

    {"MobilityKnight", Mobility[KNIGHT], 9 * 2},
    {"MobilityBishop", Mobility[BISHOP], 14 * 2},
    {"MobilityRook", Mobility[ROOK], 15 * 2},
    {"MobilityQueen", Mobility[QUEEN], (14 + 15) * 2},

    {"RookOpen", RookOpen, 2},
    {"BishopPair", &BishopPair, 2},
    {"Ahead", &Ahead, 1},

    {"Hanging", Hanging, NB_PIECE},

    {"RingAttack", RingAttack, NB_PIECE},
    {"RingDefense", RingDefense, NB_PIECE},
    {"CheckAttack", CheckAttack, 4},
    {"CheckDefense", CheckDefense, 4},
    {"XRay", &XRay[BISHOP], 3},
    {"SafetyCurveParam", SafetyCurveParam, 2},

    {"Isolated", Isolated, 2 * 2},
    {"Backward", Backward, 2 * 2},
    {"Doubled", &Doubled, 2},
    {"Shield", Shield, 4 * 6},

    {"Connected", Connected, 6 * 2},
    {"Distance", Distance, 2},

    {"PasserBonus", PasserBonus, 6 * 2},
    {"PasserAdjust", PasserAdjust, 6},
    {"FreePasser", FreePasser, 4}
};

void tune_declare()
{
    for (size_t i = 0; i < sizeof(Entries) / sizeof(Entry); i++)
        for (int j = 0; j < Entries[i].count; j++)
            printf("option name %s_%d type spin default %d min %d max %d\n",
                Entries[i].name, j, ((int *)Entries[i].values)[j], -1000000, 1000000);
}

void tune_parse(const char *fullName, int value)
{
    // split fullName = "fooBar_12", into name = "fooBar" and idx = 12
    char name[NAME_MAX_CHAR];
    int idx;

    if (sscanf(fullName, "%[a-zA-Z]_%d", name, &idx) != 2)
        return;

    // Test against each Parser Entry, and set array element on match
    for (size_t i = 0; i < sizeof(Entries) / sizeof(Entry); i++)
        if (!strcmp(name, Entries[i].name) && 0 <= idx && idx < Entries[i].count)
            ((int *)Entries[i].values)[idx] = value;
}

void tune_refresh()
{
    search_init();
    pst_init();
    eval_init();
}
