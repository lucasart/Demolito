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
#include "tune.h"

eval_t pstSeed[NB_PIECE][NB_RANK][NB_FILE / 2] = {
    {
        {{-100,-38}, {-55,-19}, {-49,-18}, {-33,-8}},
        {{ -60,-23}, {-62,-12}, {-18, -8}, { -2, 0}},
        {{ -58,-24}, {-12, -4}, { -2, -3}, { 18, 7}},
        {{ -28,-10}, { -2, -3}, { 18,  4}, { 38,17}},
        {{ -37,-11}, {  0, -3}, { 21,  6}, { 40, 9}},
        {{ -48,-17}, {-18,-12}, {  4,  1}, { 20, 8}},
        {{ -75,-23}, {-36,-15}, {-20, -6}, { -1, 2}},
        {{ -87,-17}, {-79,-24}, {-53,-16}, {-36,-9}}
    },
    {
        {{ -6,-2}, {-2, 2}, {-11, 4}, { 1,-5}},
        {{ -3,-3}, {15, 2}, {  9, 9}, {-9, 5}},
        {{ -2,-6}, { 8,-2}, {  3, 4}, {13, 3}},
        {{ 19,-7}, {-1, 7}, {  0, 0}, {-1, 4}},
        {{-11,-1}, {-2,-2}, {  0, 2}, {-2, 1}},
        {{ -3, 5}, {-4,-1}, {  1, 1}, { 0, 6}},
        {{  2,-3}, {-5,-3}, {  3,-1}, {-5, 4}},
        {{  1,10}, {-8, 1}, {  1, 4}, { 5,-3}}
    },
    {
        {{-17, 0}, { -3, 0}, {-4, 0}, {-1,-1}},
        {{ -9,-4}, {-10,-2}, {-2,-1}, {10, 2}},
        {{-15,-2}, { -2, 1}, { 0, 4}, { 6, 2}},
        {{-13,-2}, { -6, 2}, {-3,-1}, {-5, 0}},
        {{-12, 1}, { -4, 4}, { 4, 4}, { 7, 2}},
        {{-13, 0}, { -2, 0}, { 5, 3}, {11, 4}},
        {{  3,21}, { 13,12}, {11,17}, {21,16}},
        {{-15,-1}, { -3,-2}, {-2, 2}, { 6, 0}}
    },
    {
        {{-8,-34}, {-4,-27}, {-9,-25}, {-7,-12}},
        {{-1,-38}, { 0,-16}, { 0, -9}, { 2,  2}},
        {{ 2,-21}, {-2,-15}, { 0,  4}, {-3,  8}},
        {{ 2, -8}, { 0,  1}, { 1,  7}, { 0, 15}},
        {{-1,-15}, {-1, -1}, {-5,  9}, { 2, 11}},
        {{ 4,-21}, {-3, -7}, {-5, -1}, { 2, 11}},
        {{-5,-30}, { 2,-17}, {-5,-10}, {-1,  1}},
        {{ 0,-31}, { 1,-26}, { 0,-16}, { 2,-13}}
    },
    {
        {{ 59,-138}, {91,-102}, { 59,-64}, { 26,-56}},
        {{ 54, -83}, {76, -53}, { 44,-17}, {  4, -3}},
        {{ 32, -69}, {50, -17}, {  7,  1}, {-46, 26}},
        {{ 17, -24}, {24,   2}, {  1, 23}, {-40, 43}},
        {{  2, -27}, {15,   3}, {-19, 26}, {-53, 59}},
        {{ -8, -72}, {16, -33}, {-29,  1}, {-56, 20}},
        {{-16, -75}, {17, -39}, {-12,-29}, {-53,  1}},
        {{ -4,-119}, { 3,-115}, {-21,-57}, {-55,-27}}
    },
    {
        {{ 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}},
        {{-5,-4}, { 8, 3}, { 7, 4}, {-5, 6}},
        {{-9,-4}, { 3,-1}, {-5, 0}, {11,-1}},
        {{-8, 6}, { 3, 0}, { 5,-1}, {39,-6}},
        {{ 2, 3}, {-1, 4}, { 4, 3}, {22,-1}},
        {{-1, 8}, {12, 3}, {-9,-4}, {-2, 5}},
        {{-6,-3}, { 2, 1}, {-4,-2}, { 2, 1}},
        {{ 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}}
    }
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
int OurDistance[NB_RANK] = {0, 5, 13, 10, 9, 10, 7, 0};
int TheirDistance[NB_RANK] = {0, 9, 9, 9, 11, 9, 9, 0};

eval_t PasserBonus[6] = {{-3, 7}, {2, 15}, {18, 20}, {57, 61}, {150, 155}, {264, 270}};
int PasserAdjust[6] = {-1, 4, 12, 50, 72, 91};
int FreePasser[4] = {13, 14, 35, 97};

enum {NAME_MAX_CHAR = 64, PARSER_ENTRY_NB = 30};

typedef struct {
    char name[NAME_MAX_CHAR];
    int *values, count;
} ParserEntry;

ParserEntry ParserEntries[PARSER_ENTRY_NB];

static void declare(ParserEntry *pe, const char *name, void *buffer,int count)
{
    const int *values = buffer;

    // Declare UCI options for all array elements
    for (int i = 0; i < count; i++)
        printf("option name %s_%d type spin default %d min %d max %d\n",
            name, i, values[i], -1000000, 1000000);

    // Remember array information for parsing
    strcpy(pe->name, name);
    pe->values = buffer;
    pe->count = count;
}

void tune_declare_all()
{
    ParserEntry *pe = ParserEntries;

    declare(pe++, "pstKnight", pstSeed[KNIGHT], NB_SQUARE);
    declare(pe++, "pstBishop", pstSeed[BISHOP], NB_SQUARE);
    declare(pe++, "pstRook", pstSeed[ROOK], NB_SQUARE);
    declare(pe++, "pstQueen", pstSeed[QUEEN], NB_SQUARE);
    declare(pe++, "pstKing", pstSeed[KING], NB_SQUARE);
    declare(pe++, "pstPawn", pstSeed[PAWN][RANK_2], NB_SQUARE - 16);  // discard RANK_1/8

    declare(pe++, "MobilityKnight", Mobility[KNIGHT], 9 * 2);
    declare(pe++, "MobilityBishop", Mobility[BISHOP], 14 * 2);
    declare(pe++, "MobilityRook", Mobility[ROOK], 15 * 2);
    declare(pe++, "MobilityQueen", Mobility[QUEEN], (14 + 15) * 2);

    declare(pe++, "RookOpen", RookOpen, 2);
    declare(pe++, "BishopPair", &BishopPair, 2);
    declare(pe++, "Ahead", &Ahead, 1);

    declare(pe++, "Hanging", Hanging, NB_PIECE);

    declare(pe++, "RingAttack", RingAttack, NB_PIECE);
    declare(pe++, "RingDefense", RingDefense, NB_PIECE);
    declare(pe++, "CheckAttack", CheckAttack, 4);
    declare(pe++, "CheckDefense", CheckDefense, 4);
    declare(pe++, "XRay", &XRay[BISHOP], 3);
    declare(pe++, "SafetyCurveParam", SafetyCurveParam, 2);

    declare(pe++, "Isolated", Isolated, 2 * 2);
    declare(pe++, "Backward", Backward, 2 * 2);
    declare(pe++, "Doubled", &Doubled, 2);
    declare(pe++, "Shield", Shield, 4 * 6);

    declare(pe++, "Connected", Connected, 6 * 2);
    declare(pe++, "OurDistance", &OurDistance[RANK_2], NB_RANK - 2);  // discard RANK_1/8
    declare(pe++, "TheirDistance", &TheirDistance[RANK_2], NB_RANK - 2);  // discard RANK_1/8

    declare(pe++, "PasserBonus", PasserBonus, 6 * 2);
    declare(pe++, "PasserAdjust", PasserAdjust, 6);
    declare(pe++, "FreePasser", FreePasser, 4);

    assert(pe == &ParserEntries[PARSER_ENTRY_NB]);
}

void tune_parse_all(const char *fullName, int value)
{
    // split fullName = "fooBar_12", into name = "fooBar" and idx = 12
    char name[NAME_MAX_CHAR];
    int idx;

    if (sscanf(fullName, "%[a-zA-Z]_%d", name, &idx) != 2)
        return;

    // Test against each Parser Entry, and set array element on match
    for (int i = 0; i < PARSER_ENTRY_NB; i++)
        if (!strcmp(name, ParserEntries[i].name) && 0 <= idx && idx < ParserEntries[i].count)
            ParserEntries[i].values[idx] = value;
}
