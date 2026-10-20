/* Global data of progress.c, generated from the executable by tools/gendata.py. */
#include "functions.h"

u_char loadInstructionLine0[32] = {
    32, 32, 76, 79, 65, 68, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 51, 51, 32, 32, 32, 32, 51, 51,
    32, 32, 32, 32, 51, 51, 0, 0,
};
u_char loadInstructionLine1[12] = { 76, 79, 67, 65, 84, 73, 79, 78, 0, 0, 0, 0 };
u_char shipTeamNames0[12] = { 65, 71, 32, 83, 89, 83, 84, 69, 77, 83, 0, 0 };
u_char track6DescriptionLine0[16] = { 83, 79, 85, 84, 72, 32, 80, 65, 67, 73, 70, 73, 67, 0, 0, 0 };
u_char track7DescriptionLine0[16] = { 78, 79, 82, 84, 72, 32, 65, 77, 69, 82, 73, 67, 65, 0, 0, 0 };
u_char track17DescriptionLine0[176] = {
    66, 76, 65, 67, 75, 32, 83, 69, 65, 0, 0, 0, 119, 97, 100, 102, 105, 108, 101, 46, 99, 58, 111,
    112, 101, 110, 119, 97, 100, 102, 105, 108, 101, 40, 41, 58, 32, 99, 97, 110, 39, 116, 32, 111,
    112, 101, 110, 32, 37, 115, 10, 0, 119, 97, 100, 102, 105, 108, 101, 46, 99, 58, 111, 112, 101,
    110, 119, 97, 100, 102, 105, 108, 101, 110, 118, 99, 40, 41, 58, 32, 99, 97, 110, 39, 116, 32,
    111, 112, 101, 110, 32, 37, 115, 10, 0, 0, 224, 152, 6, 128, 52, 153, 6, 128, 136, 155, 6, 128,
    136, 155, 6, 128, 136, 155, 6, 128, 136, 153, 6, 128, 216, 153, 6, 128, 44, 154, 6, 128, 136,
    155, 6, 128, 136, 155, 6, 128, 136, 155, 6, 128, 136, 155, 6, 128, 132, 154, 6, 128, 136, 155,
    6, 128, 136, 155, 6, 128, 136, 155, 6, 128, 220, 154, 6, 128, 136, 155, 6, 128, 136, 155, 6,
    128, 52, 155, 6, 128,
};
s32 wadHandle;
u8 vsyncBusy;
s32 *particleLife;
s32 *particleX;
s32 *particleY;
LINE_F2 *linePrims;
int ownShip;
s32 *particleVelX;
s32 *particleVelY;
volatile s32 wadRemaining;
s32 loadPercent;
POLY_F3 *particlePolys;
short CurrentScreen;
s32 scanPhase;
s32 wadSize;
u8 *wadCache;
u8 barHeight[128];
char numberText[256];
