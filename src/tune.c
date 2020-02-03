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

int RingAttack[4] = {38, 51, 77, 75};
int RingDefense[4] = {21, 29, 52, 45};
int CheckAttack[4] = {84, 102, 108, 106};
int CheckDefense[4] = {31, 55, 40, 48};
int XRay[4] = {0, 83, 116, 85};
int SafetyCurveParam[2] = {1062, 800};

eval_t Isolated[2] = {{15, 25}, {41, 25}};
eval_t Backward[2] = {{12, 18}, {37, 15}};
eval_t Doubled = {28, 36};
int Shield[4][NB_RANK] = {
    {0, 21, 19, 8, 12, 17, 11, 0},
    {0, 31, 23, 5, 7, 5, 3, 0},
    {0, 25, 17, 12, 14, 17, 13, 0},
    {0, 25, 18, 16, 10, 7, 6, 0}
};
eval_t Connected[] = {{8, -4}, {18, 2}, {20, 7}, {42, 21}, {34, 58}, {48, 68}};
int OurDistance[NB_RANK] = {0, 5, 13, 10, 9, 10, 7, 0};
int TheirDistance[NB_RANK] = {0, 9, 9, 9, 11, 9, 9, 0};

eval_t PasserBonus[6] = {{-3, 7}, {2, 15}, {18, 20}, {57, 61}, {150, 155}, {264, 270}};
int PasserAdjust[6] = {-1, 4, 12, 50, 72, 91};
int FreePasser[4] = {13, 14, 35, 97};

static void declare(const char *name, const void *buffer, int count)
{
    const int *values = buffer;

    for (int i = 0; i < count; i++)
        printf("option name %s_%d type spin default %d min %d max %d\n",
            name, i, values[i], -1000000, 1000000);
}

// Example: expectedName = "fooBar", name = "fooBar", index = 12, buffer = &fooBar
// Verifies that 0 <= 12 < count, and sets fooBar[12] = value
static void parse(const char *expectedName, const char *name, int idx, int value, void *buffer,
    int count)
{
    int *values = buffer;

    if (!strcmp(name, expectedName) && 0 <= idx && idx < count)
        values[idx] = value;
}

void tune_declare_all()
{
    declare("pstKnight", pstSeed[KNIGHT], NB_SQUARE);
    declare("pstBishop", pstSeed[BISHOP], NB_SQUARE);
    declare("pstRook", pstSeed[ROOK], NB_SQUARE);
    declare("pstQueen", pstSeed[QUEEN], NB_SQUARE);
    declare("pstKing", pstSeed[KING], NB_SQUARE);
    declare("pstPawn", pstSeed[PAWN][RANK_2], NB_SQUARE - 16);

    declare("MobilityKnight", Mobility[KNIGHT], 9 * 2);
    declare("MobilityBishop", Mobility[BISHOP], 14 * 2);
    declare("MobilityRook", Mobility[ROOK], 15 * 2);
    declare("MobilityQueen", Mobility[QUEEN], (14 + 15) * 2);

    declare("RookOpen", RookOpen, 2);
    declare("BishopPair", &BishopPair, 2);
    declare("Ahead", &Ahead, 1);

    declare("Hanging", Hanging, NB_PIECE);

    declare("RingAttack", RingAttack, 4);
    declare("RingDefense", RingDefense, 4);
    declare("CheckAttack", CheckAttack, 4);
    declare("CheckDefense", CheckDefense, 4);
    declare("XRay", &XRay[BISHOP], 3);
    declare("SafetyCurveParam", SafetyCurveParam, 2);

    declare("Isolated", Isolated, 2 * 2);
    declare("Backward", Backward, 2 * 2);
    declare("Doubled", &Doubled, 2);
    declare("Shield", Shield, 4 * NB_RANK);
    declare("Connected", Connected, 6 * 2);
    declare("OurDistance", OurDistance, NB_RANK);
    declare("TheirDistance", TheirDistance, NB_RANK);

    declare("PasserBonus", PasserBonus, 6 * 2);
    declare("PasserAdjust", PasserAdjust, 6);
    declare("FreePasser", FreePasser, 4);
}

void tune_parse_all(const char *fullName, int value)
{
    // split fullName = "fooBar_12", into name = "fooBar" and idx = 12
    char name[64];
    int idx;

    if (sscanf(fullName, "%[a-zA-Z]_%d", name, &idx) != 2)
        return;

    parse("pstKnight", name, idx, value, pstSeed[KNIGHT], NB_SQUARE);
    parse("pstBishop", name, idx, value, pstSeed[BISHOP], NB_SQUARE);
    parse("pstRook", name, idx, value, pstSeed[ROOK], NB_SQUARE);
    parse("pstQueen", name, idx, value, pstSeed[QUEEN], NB_SQUARE);
    parse("pstPawn", name, idx, value, pstSeed[PAWN][RANK_2], NB_SQUARE - 16);

    parse("MobilityKnight", name, idx, value, Mobility[KNIGHT], 9 * 2);
    parse("MobilityBishop", name, idx, value, Mobility[BISHOP], 14 * 2);
    parse("MobilityRook", name, idx, value, Mobility[ROOK], 15 * 2);
    parse("MobilityQueen", name, idx, value, Mobility[QUEEN], (14 + 15) * 2);

    parse("RookOpen", name, idx, value, RookOpen, 2);
    parse("BishopPair", name, idx, value, &BishopPair, 2);
    parse("Ahead", name, idx, value, &Ahead, 1);
    parse("Hanging", name, idx, value, Hanging, NB_PIECE);

    parse("RingAttack", name, idx, value, RingAttack, 4);
    parse("RingDefense", name, idx, value, RingDefense, 4);
    parse("CheckAttack", name, idx, value, CheckAttack, 4);
    parse("CheckDefense", name, idx, value, CheckDefense, 4);
    parse("XRay", name, idx, value, &XRay[BISHOP], 3);
    parse("SafetyCurveParam", name, idx, value, SafetyCurveParam, 2);

    parse("Isolated", name, idx, value, Isolated, 2 * 2);
    parse("Backward", name, idx, value, Backward, 2 * 2);
    parse("Doubled", name, idx, value, &Doubled, 2);
    parse("Shield", name, idx, value, Shield, 4 * NB_RANK);
    parse("Connected", name, idx, value, Connected, 6 * 2);
    parse("OurDistance", name, idx, value, OurDistance, NB_RANK);
    parse("TheirDistance", name, idx, value, TheirDistance, NB_RANK);
 
    parse("PasserBonus", name, idx, value, PasserBonus, 6 * 2);
    parse("PasserAdjust", name, idx, value, PasserAdjust, 6);
    parse("FreePasser", name, idx, value, FreePasser, 4);
}
