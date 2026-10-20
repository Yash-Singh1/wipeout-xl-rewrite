/* Global data of drone.c, generated from the executable by tools/gendata.py. */
#include "functions.h"

/* Types that drone.c declares privately. */
typedef struct ClassBonus {
    short value[4];
} ClassBonus;

u_short classSpeedLimit[4] = { 0x3e80, 0x4650, 0x4e20, 0x55f0 };
short classSpeedMargin[4] = { 1200, 1600, 2100, 2600 };
u_short classAccel[4] = { 100, 120, 140, 160 };
short classSteer[4] = { 1200, 1600, 2100, 2200 };
ClassBonus droneInitVector[1] = { { { 80, 140, 200, 210 } } };
u_char sfxInitialized = 0;
u_char reverbReserved = 0;
u_char reverbActive = 0;
u_char callbackPending = 0;
u_char sfxGain = 255;
u_char sfxEnabled = 0;
u_char cdVolume = 255;
u_char globalVolume = 255;
int totalSampleMem = 0;
u_char nextVoice = 0;
s32 scoreTabFrame = 0;
u8 msgSelect[8] = { 83, 69, 76, 69, 67, 84, 0, 1 };
RaceLine raceLines[1];
VECTOR racingLine[599];
short shipOrder[14][2];
